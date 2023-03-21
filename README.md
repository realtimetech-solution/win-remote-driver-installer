# win-remote-driver-installer
Deploy and Install and Run driver remotely via TCP/IP on windows.

## wrdi-server
```
wrdi-server.exe <Host> <Port> <Working Directory>

Example:
wrdi-server-x64.exe 127.0.0.1 2828 tempDirectory/
```

## wrdi-client
```
wrdi-client.exe <Server> <Port> <Driver Name> <Upload Directory or File> <Install File>

Example:
wrdi-client-x64.exe 127.0.0.1 2828 TestDriver Debug/x64/ Debug/x64/testdriver.sys
wrdi-client-x64.exe 127.0.0.1 2828 TestDriver Debug/x64/ Debug/x64/testdriver.inf
wrdi-client-x64.exe 127.0.0.1 2828 TestDriver Debug/x64/testdriver.sys Debug/x64/testdriver.sys
```
