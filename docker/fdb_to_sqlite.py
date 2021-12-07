"""Module for converting a FDB database to a SQLite database"""
import argparse
import os
import sqlite3
import struct
from collections import OrderedDict

# There seems to be no difference between 4 and 8, but just in case there is I'm keeping that type info
SQLITE_TYPE = {}
SQLITE_TYPE[0] = "none"
SQLITE_TYPE[1] = "int32"
SQLITE_TYPE[3] = "real"
SQLITE_TYPE[4] = "text_4"
SQLITE_TYPE[5] = "int_bool"
SQLITE_TYPE[6] = "int64"
SQLITE_TYPE[8] = "text_8"


def pointer_scope(func):
    """The FDB format has a lot of pointers to structures, so this decorator automatically reads the pointer, seeks to the pointer position, calls the function, and seeks back."""

    def wrapper(self, *args, **kwargs):
        pointer = kwargs.get("pointer")

        if pointer == None:
            pointer = self._read_int32()
        else:
            del kwargs["pointer"]

        if pointer == -1:
            return

        current_pos = self.fdb.tell()
        self.fdb.seek(pointer)

        result = func(self, *args, **kwargs)

        self.fdb.seek(current_pos)
        return result
    return wrapper

# I'm using a class for this to save things like the fdb and the sqlite without using globals


class convert:
    def __init__(self, in_file, out_file=None, add_link_info=False):
        self.add_link_info = add_link_info
        if out_file == None:
            out_file = os.path.splitext(os.path.basename(in_file))[
                0] + ".sqlite"

        if os.path.exists(out_file):
            os.remove(out_file)

        self.fdb = open(in_file, "rb")
        self.sqlite = sqlite3.connect(out_file)

        self._read()
        print("-"*79)
        print("Finished converting database!")
        print("-"*79)

        self.sqlite.commit()
        self.sqlite.close()
        self.fdb.close()

    def _read(self):
        number_of_tables = self._read_int32()
        self._read_tables(number_of_tables)

    @pointer_scope
    def _read_tables(self, number_of_tables):
        for table_struct_index in range(number_of_tables):
            table_name, number_of_columns = self._read_column_header()
            print("[%2i%%] Reading table %s" %
                  (table_struct_index*100//number_of_tables, table_name))
            self._read_row_header(table_name, number_of_columns)

    @pointer_scope
    def _read_column_header(self):
        number_of_columns = self._read_int32()
        table_name = self._read_string()
        columns = self._read_columns(number_of_columns)

        sql = "create table if not exists '%s' (%s)" % \
            (table_name,	", ".join(
                ["'%s' %s" % (col, SQLITE_TYPE[columns[col]]) for col in columns]))

        self.sqlite.execute(sql)
        return table_name, len(columns)

    @pointer_scope
    def _read_columns(self, number_of_columns):
        columns = OrderedDict()

        for _ in range(number_of_columns):
            data_type = self._read_int32()
            name = self._read_string()
            columns[name] = data_type

        if self.add_link_info:
            columns["_linked_from"] = 1
            columns["_does_link"] = 5
            columns["_invalid"] = 5

        return columns

    @pointer_scope
    def _read_row_header(self, table_name, number_of_columns):
        number_of_allocated_rows = self._read_int32()
        if number_of_allocated_rows != 0:
            # assert power of 2 allocation size
            assert number_of_allocated_rows & (
                number_of_allocated_rows - 1) == 0

        self.sqlite.executemany("insert into '%s' values (%s)" % (table_name, ", ".join(
            ["?"] * number_of_columns)), self._read_rows(number_of_allocated_rows, number_of_columns))

    @pointer_scope
    def _read_rows(self, number_of_allocated_rows, number_of_columns):
        rowid = 0
        percent_read = -1  # -1 so 0% is displayed as new
        for row in range(number_of_allocated_rows):
            new_percent_read = row*100//number_of_allocated_rows
            if new_percent_read > percent_read:
                percent_read = new_percent_read
                print("[%2i%%] Reading rows" % percent_read, end="\r")

            row_pointer = self._read_int32()
            if row_pointer == -1:
                if self.add_link_info:
                    # invalid row
                    yield (None,) * (number_of_columns-1) + (True,)
                rowid += 1
            else:
                linked_rows, rowid = self._read_row(rowid, pointer=row_pointer)
                for values in linked_rows:
                    yield values

    @pointer_scope
    def _read_row(self, rowid):
        rows = []
        linked_from = None
        while True:
            row_values = self._read_row_info()
            linked = self._read_int32()
            if self.add_link_info:
                row_values.append(linked_from)
                row_values.append(linked != -1)
                row_values.append(False)  # valid row
            rows.append(row_values)

            rowid += 1

            if linked == -1:
                break

            self.fdb.seek(linked)
            linked_from = rowid

        return rows, rowid

    @pointer_scope
    def _read_row_info(self):
        number_of_columns = self._read_int32()
        return self._read_row_values(number_of_columns)

    @pointer_scope
    def _read_row_values(self, number_of_columns):
        values = []

        for _ in range(number_of_columns):
            data_type = self._read_int32()
            if data_type == 0:
                assert self.fdb.read(4) == b"\0\0\0\0"
                value = None
            elif data_type == 1:
                value = self._read_int32()
            elif data_type == 3:
                value = struct.unpack("f", self.fdb.read(4))[0]
            elif data_type in (4, 8):
                value = self._read_string()
            elif data_type == 5:
                value = struct.unpack("?xxx", self.fdb.read(4))[0]
            elif data_type == 6:
                value = self._read_int64()
            else:
                raise NotImplementedError(data_type)

            values.append(value)
        return values

    def _read_int32(self):
        return struct.unpack("i", self.fdb.read(4))[0]

    @pointer_scope
    def _read_string(self):
        str_bytes = bytearray()
        while True:
            byte = self.fdb.read(1)
            if byte == b"\0":
                break
            str_bytes += byte
        return str_bytes.decode("latin1")

    @pointer_scope
    def _read_int64(self):
        return struct.unpack("q", self.fdb.read(8))[0]


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("fdb_path")
    parser.add_argument("--sqlite_path")
    parser.add_argument("--add_link_info", action="store_true")
    args = parser.parse_args()
    convert(args.fdb_path, args.sqlite_path, args.add_link_info)
