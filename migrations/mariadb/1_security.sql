-- root password is set by the docker image / env variables, so no need to change it here

-- enable unix_socket authentication - currently disabled because the server may need the open port
-- UPDATE mysql.global_priv SET priv=json_set(priv, '$.password_last_changed', UNIX_TIMESTAMP(), '$.plugin', 'mysql_native_password', '$.authentication_string', 'invalid', '$.auth_or', json_array(json_object(), json_object('plugin', 'unix_socket'))) WHERE User='root';

-- remove anonymous users
DELETE FROM mysql.global_priv WHERE User='';

-- remove remote root
DELETE FROM mysql.global_priv WHERE User='root' AND Host NOT IN ('localhost', '127.0.0.1', '::1');

-- remove test db and access to it
DROP DATABASE IF EXISTS test;
DELETE FROM mysql.db WHERE Db='test' OR Db='test\\_%';

-- reload privileges
FLUSH PRIVILEGES;

