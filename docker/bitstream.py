"""
Module for sequential reading (ReadStream) and writing (WriteStream) from/to bytes.
Also includes objects for converting datatypes from/to bytes, similar to the standard library struct module.
"""

# https://github.com/lcdr/bitstream/blob/master/bitstream/__init__.py

import math
import struct
from abc import ABC, abstractmethod
from typing import AnyStr, ByteString, cast, Generic, overload, SupportsBytes, Type, TypeVar

T = TypeVar('T')

class _Struct(Generic[T]):
	_struct: struct.Struct

	def __new__(cls, value: T) -> bytes:
		return cls._struct.pack(value)

	def __str__(self) -> str:
		return "<Struct %s>" % _Struct._struct.format

	@classmethod
	def deserialize(cls, stream: "ReadStream") -> T:
		return cast(T, cls._struct.unpack(stream.read(bytes, length=cls._struct.size))[0])

class IntStruct(_Struct[int]):
	pass

class UnsignedIntStruct(IntStruct):
	@classmethod
	def deserialize_compressed(cls, stream: "ReadStream") -> int:
		number_of_bytes = cls._struct.size
		current_byte = number_of_bytes - 1

		while current_byte > 0:
			if stream.read(c_bit):
				current_byte -= 1
			else:
				# Read the rest of the bytes
				read = stream.read(bytes, length=current_byte + 1) + bytes(number_of_bytes - current_byte - 1)
				return cast(int, cls._struct.unpack(read)[0])

		# All but the first bytes are 0. If the upper half of the last byte is a 0 (positive) or 16 (negative) then what we read will be a 1 and the remaining 4 bits.
		# Otherwise we read a 0 and the 8 bits
		if stream.read(c_bit):
			start = bytes([stream.read_bits(4)])
		else:
			start = stream.read(bytes, length=1)
		read = start + bytes(number_of_bytes - current_byte - 1)
		return cast(int, cls._struct.unpack(read)[0])

class SignedIntStruct(IntStruct):
	pass

class c_bool(_Struct[bool]):
	_struct = struct.Struct("<?")

class c_float(_Struct[float]):
	_struct = struct.Struct("<f")

class c_double(_Struct[float]):
	_struct = struct.Struct("<d")

class c_int(SignedIntStruct):
	_struct = struct.Struct("<i")

class c_uint(UnsignedIntStruct):
	_struct = struct.Struct("<I")


class c_byte(SignedIntStruct):
	_struct = struct.Struct("<b")

class c_ubyte(UnsignedIntStruct):
	_struct = struct.Struct("<B")

class c_short(SignedIntStruct):
	_struct = struct.Struct("<h")

class c_ushort(UnsignedIntStruct):
	_struct = struct.Struct("<H")

class c_long(SignedIntStruct):
	_struct = struct.Struct("<l")

class c_ulong(UnsignedIntStruct):
	_struct = struct.Struct("<L")

class c_longlong(SignedIntStruct):
	_struct = struct.Struct("<q")

class c_ulonglong(UnsignedIntStruct):
	_struct = struct.Struct("<Q")

c_int8 = c_byte
c_uint8 = c_ubyte
c_int16 = c_short
c_uint16 = c_ushort
c_int32 = c_long
c_uint32 = c_ulong
c_int64 = c_longlong
c_uint64 = c_ulonglong

class c_bit:
	def __init__(self, boolean: bool):
		self.value = boolean

class Serializable(ABC):
	"""By inheriting from this class you can create types which you can pass to the read/write bitstream functions."""
	@abstractmethod
	def serialize(self, stream: "WriteStream") -> None:
		"""Write this object to the bitstream."""
		pass

	@classmethod
	@abstractmethod
	def deserialize(cls, stream: "ReadStream") -> "Serializable":
		"""Create a new object from the bitstream."""
		pass

S = TypeVar('S', bound=Serializable)

class ReadStream:
	"""Allows simple sequential reading from bytes."""
	_data: bytes

	def __init__(self, data: bytes, unlocked: bool=False):
		self._data = data
		self._unlocked = unlocked
		self._read_offset = 0

	@property
	def read_offset(self) -> int:
		if not self._unlocked:
			raise RuntimeError("access to read offset on locked stream")
		return self._read_offset

	@read_offset.setter
	def read_offset(self, value: int) -> None:
		if not self._unlocked:
			raise RuntimeError("access to read offset on locked stream")
		self._read_offset = value

	def skip_read(self, byte_length: int) -> None:
		"""Skips reading byte_length number of bytes."""
		self._read_offset += byte_length * 8

	@overload
	def read(self, arg_type: Type[_Struct[T]]) -> T:
		pass

	@overload
	def read(self, arg_type: Type[c_bit]) -> bool:
		pass

	@overload
	def read(self, arg_type: Type[S]) -> S:
		pass

	@overload
	def read(self, arg_type: Type[bytes], length: int) -> bytes:
		pass

	@overload
	def read(self, arg_type: Type[bytes], allocated_length: int=None, length_type: Type[UnsignedIntStruct]=None) -> bytes:
		pass

	@overload
	def read(self, arg_type: Type[str], allocated_length: int=None, length_type: Type[UnsignedIntStruct]=None) -> str:
		pass

	def read(self, arg_type, length=None, allocated_length=None, length_type=None):
		"""
		Read a value of type arg_type from the bitstream.
		allocated_length is for fixed-length strings.
		length_type is for variable-length strings.
		"""
		if issubclass(arg_type, _Struct):
			return arg_type.deserialize(self)
		if issubclass(arg_type, c_bit):
			return self._read_bit()
		if issubclass(arg_type, Serializable):
			return arg_type.deserialize(self)
		if allocated_length is not None or length_type is not None:
			return self._read_str(arg_type, allocated_length, length_type)
		if issubclass(arg_type, bytes):
			return self._read_bytes(length)
		raise TypeError(arg_type)

	def _read_str(self, arg_type: Type[AnyStr], allocated_length: int=None, length_type: Type[UnsignedIntStruct]=None) -> AnyStr:
		if issubclass(arg_type, str):
			char_size = 2
		else:
			char_size = 1

		if length_type is not None:
			# Variable-length string
			length = self.read(length_type)
			value = self._read_bytes(length*char_size)
		elif allocated_length is not None:
			# Fixed-length string
			value = self._read_bytes(allocated_length*char_size)
			# find null terminator
			for i in range(len(value)):
				char = value[i*char_size:(i+1)*char_size]
				if char == bytes(char_size):
					value = value[:i*char_size]
					break
			else:
				raise RuntimeError("String doesn't have null terminator")
		else:
			raise ValueError

		if issubclass(arg_type, str):
			return value.decode("utf-16-le")
		return value

	def _read_bit(self) -> bool:
		bit = self._data[self._read_offset // 8] & 0x80 >> self._read_offset % 8 != 0
		self._read_offset += 1
		return bit

	def read_bits(self, number_of_bits: int) -> int:
		assert 0 < number_of_bits < 8

		output = (self._data[self._read_offset // 8] << self._read_offset % 8) & 0xff # First half
		if self._read_offset % 8 != 0 and number_of_bits > 8 - self._read_offset % 8: # If we have a second half, we didn't read enough bytes in the first half
			output |= self._data[self._read_offset // 8 + 1] >> 8 - self._read_offset % 8 # Second half (overlaps byte boundary)
		output >>= 8 - number_of_bits
		self._read_offset += number_of_bits
		return output

	def _read_bytes(self, length: int) -> bytes:
		if self._read_offset % 8 == 0:
			num_bytes_read = length
		else:
			num_bytes_read = length+1

		# check whether there is enough left to read
		if len(self._data) - self._read_offset//8 < num_bytes_read:
			raise EOFError("Trying to read %i bytes but only %i remain" % (num_bytes_read, len(self._data) - self._read_offset // 8))

		if self._read_offset % 8 == 0:
			output = self._data[self._read_offset // 8:self._read_offset // 8 + num_bytes_read]
		else:
			# data is shifted
			# clear the part before the struct

			firstbyte = self._data[self._read_offset // 8] & ((1 << 8 - self._read_offset % 8) - 1)
			output = firstbyte.to_bytes(1, "big") + self._data[self._read_offset // 8 + 1:self._read_offset // 8 + num_bytes_read]
			# shift back
			output = (int.from_bytes(output, "big") >> (8 - self._read_offset % 8)).to_bytes(length, "big")
		self._read_offset += length * 8
		return output

	def read_compressed(self, arg_type: Type[UnsignedIntStruct]) -> int:
		return arg_type.deserialize_compressed(self)

	def read_remaining(self) -> bytes:
		return self._read_bytes(len(self._data) - int(math.ceil(self._read_offset / 8)))

	def align_read(self) -> None:
		if self._read_offset % 8 != 0:
			self._read_offset += 8 - self._read_offset % 8

	def all_read(self) -> bool:
		# This is not accurate to the bit, just to the byte
		return math.ceil(self._read_offset / 8) == len(self._data)

# Note: a ton of the logic here assumes that the write offset is never moved back, that is, that you never overwrite things
# Doing so may break everything
class WriteStream(SupportsBytes):
	"""Allows simple sequential writing to bytes."""
	_data: bytearray

	def __init__(self) -> None:
		self._data = bytearray()
		self._write_offset = 0
		self._was_cast_to_bytes = False

	def __bytes__(self) -> bytes:
		if self._was_cast_to_bytes:
			raise RuntimeError("WriteStream can only be cast to bytes once")
		self._was_cast_to_bytes = True
		return bytes(self._data)

	@overload
	def write(self, arg: ByteString) -> None:
		pass

	@overload
	def write(self, arg: _Struct) -> None:
		pass

	@overload
	def write(self, arg: c_bit) -> None:
		pass

	@overload
	def write(self, arg: Serializable) -> None:
		pass

	@overload
	def write(self, arg: AnyStr, allocated_length: int=None, length_type: Type[UnsignedIntStruct]=None) -> None:
		pass

	def write(self, arg, allocated_length=None, length_type=None):
		"""
		Write a value to the bitstream.
		allocated_length is for fixed-length strings.
		length_type is for variable-length strings.
		"""
		if isinstance(arg, c_bit):
			self._write_bit(arg.value)
			return
		if isinstance(arg, Serializable):
			arg.serialize(self)
			return
		if allocated_length is not None or length_type is not None:
			self._write_str(arg, allocated_length, length_type)
			return
		if isinstance(arg, (bytes, bytearray)):
			self._write_bytes(arg)
			return

		raise TypeError(arg)

	def _write_str(self, str_: AnyStr, allocated_length: int=None, length_type: Type[UnsignedIntStruct]=None) -> None:
		# possibly include default encoded length for non-variable-length strings (seems to be 33)
		if isinstance(str_, str):
			encoded_str = str_.encode("utf-16-le")
		else:
			encoded_str = str_

		if length_type is not None:
			# Variable-length string
			self.write(length_type(len(str_))) # note: there's also a version that uses the length of the encoded string, should that be used?
		elif allocated_length is not None:
			# Fixed-length string
			# null terminator
			if isinstance(str_, str):
				char_size = 2
			else:
				char_size = 1

			if len(str_)+1 > allocated_length:
				raise ValueError("String too long!")
			encoded_str += bytes(allocated_length*char_size-len(encoded_str))
		self._write_bytes(encoded_str)

	def _write_bit(self, bit: bool) -> None:
		self._alloc_bits(1)
		if bit: # we don't actually have to do anything if the bit is 0
			self._data[self._write_offset//8] |= 0x80 >> self._write_offset % 8

		self._write_offset += 1

	def write_bits(self, value: int, number_of_bits: int) -> None:
		assert 0 < number_of_bits < 8
		self._alloc_bits(number_of_bits)

		if number_of_bits < 8: # In the case of a partial byte, the bits are aligned from the right (bit 0) rather than the left (as in the normal internal representation)
			value = value << (8 - number_of_bits) & 0xff # Shift left to get the bits on the left, as in our internal representation
		if self._write_offset % 8 == 0:
			self._data[self._write_offset//8] = value
		else:
			self._data[self._write_offset//8] |= value >> self._write_offset % 8 # First half
			if 8 - self._write_offset % 8 < number_of_bits: # If we didn't write it all out in the first half (8 - self._write_offset % 8 is the number we wrote in the first half)
				self._data[self._write_offset//8 + 1] = (value << 8 - self._write_offset % 8) & 0xff # Second half (overlaps byte boundary)

		self._write_offset += number_of_bits

	def _write_bytes(self, byte_arg: bytes) -> None:
		if self._write_offset % 8 == 0:
			self._data[self._write_offset//8:self._write_offset//8+len(byte_arg)] = byte_arg
		else:
			# shift new input to current shift
			new = (int.from_bytes(byte_arg, "big") << (8 - self._write_offset % 8)).to_bytes(len(byte_arg)+1, "big")
			# update current byte
			self._data[self._write_offset//8] |= new[0]
			# add rest
			self._data[self._write_offset//8+1:self._write_offset//8+1+len(byte_arg)] = new[1:]
		self._write_offset += len(byte_arg)*8

	@overload
	def write_compressed(self, byte_arg: UnsignedIntStruct) -> None:
		pass

	@overload
	def write_compressed(self, byte_arg: bytes) -> None:
		pass

	def write_compressed(self, byte_arg) -> None:
		current_byte = len(byte_arg) - 1

		# Write upper bytes with a single 1
		# From high byte to low byte, if high byte is 0 then write 1. Otherwise write 0 and the remaining bytes
		while current_byte > 0:
			is_zero = byte_arg[current_byte] == 0
			self._write_bit(is_zero)
			if not is_zero:
				# Write the remainder of the data
				self._write_bytes(byte_arg[:current_byte + 1])
				return
			current_byte -= 1

		# If the upper half of the last byte is 0 then write 1 and the remaining 4 bits. Otherwise write 0 and the 8 bits.

		is_zero = byte_arg[0] & 0xF0 == 0x00
		self._write_bit(is_zero)
		if is_zero:
			self.write_bits(byte_arg[0], 4)
		else:
			self._write_bytes(byte_arg[:1])

	def align_write(self) -> None:
		"""Align the write offset to the byte boundary."""
		if self._write_offset % 8 != 0:
			self._alloc_bits(8 - self._write_offset % 8)
			self._write_offset += 8 - self._write_offset % 8

	def _alloc_bits(self, number_of_bits: int) -> None:
		bytes_to_allocate: int = math.ceil((self._write_offset + number_of_bits) / 8) - len(self._data)
		if bytes_to_allocate > 0:
			self._data += bytes(bytes_to_allocate)