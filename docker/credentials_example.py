import os

DB_URL = f'mysql+pymysql://{os.environ["DATABASE_USER"]}:{os.environ["DATABASE_PASSWORD"]}@{os.environ["DATABASE_HOST"]}/{os.environ["DATABASE"]}'
