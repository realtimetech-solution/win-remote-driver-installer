# win-remote-driver-installer
Deploy and Install and Run driver remotely via TCP/IP on windows.

## wrdi-server
### Command Example
```
wrdi-server.exe <Configuration INI File>

Example:
wrdi-server-x64.exe server_config.ini
```

### Configuration Example
```
[Host]
Address             = 127.0.0.1
Port                = 2828

[Environment]
WorkingDirectory    = temp/
```

## wrdi-client
### Command Example
```
wrdi-client.exe <Configuration INI File>

Example:
wrdi-client-x64.exe client_config.ini
```

### Configuration Example
```
[Server]
Address         = 192.168.0.11
Port            = 2828

[Driver]
Name            = MyDriver

[Upload]
DirectoryPath   = MyDriver/

[Installation]
FilePath        = MyDriver/Release/x64/driver.sys       ; *.sys, *.ini

[Execution]
FilePath        = MyDriver/Test/driver_tester_x64.exe   ; Optional
```
