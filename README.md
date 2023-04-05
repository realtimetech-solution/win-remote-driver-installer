# win-remote-driver-installer
Deploy and Install and Run driver remotely via TCP/IP on windows.

## wrdi-server command
```
wrdi-server.exe <Server configuration INI file>

Example:
wrdi-server-x64.exe server_config.ini
```

## wrdi-server config example
```
[CONNECTION]
server_ip = 127.0.0.1
port = 2828

[FILE]
working_directory = test
```

## wrdi-client
```
wrdi-client.exe <Client configuration INI file>

Example:
wrdi-client-x64.exe client_config.ini
```

## wrdi-client config example
```
[CONNECTION]
server_ip = 127.0.0.1
port = 2828

[FILE]
driver_name = MyDriver
upload_target = MyDriver/Release/x64
install_file = MyDriver/Release/x64/driver.sys
example_binary = usage_ex.exe	;optional : leave as blank
```
