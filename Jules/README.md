# Jules Development Files

This folder contains the latest development work for the cTrader Zorro Plugin.

## Contents

### Source Files (`src/`)
All C++ implementation files for the Zorro DLL with cTrader API integration:
- `main.cpp` - Core Zorro API implementation (17 functions)
- `symbols.cpp` - Enhanced symbol lookup with debug logging
- `account.cpp` - Account info handler
- `history.cpp` - Historical data retrieval
- `trading.cpp` - Order management
- `auth.cpp` - OAuth2 authentication
- `network.cpp` - WebSocket communication
- `http_api.cpp` - HTTP API wrapper
- `oauth_utils.cpp` - OAuth utility functions
- `csv_loader.cpp` - CSV configuration loader
- `reconnect.cpp` - Connection management
- `utils.cpp` - Logging and utility functions

### Header Files (`include/`)
All C++ header files with declarations and interfaces:
- `globals.h` - Global structures and constants
- `symbols.h` - Symbol management interface
- `account.h` - Account information interface
- `history.h` - Historical data interface
- `trading.h` - Trading operations interface
- `auth.h` - Authentication interface
- `network.h` - Network communication interface
- `http_api.h` - HTTP API interface
- `oauth_utils.h` - OAuth utility interface
- `csv_loader.h` - CSV loader interface
- `reconnect.h` - Reconnection interface
- `utils.h` - Utility functions interface

## Development Context

**Project**: Zorro DLL development with cTrader API integration
**Language**: C#/C++ (Hybrid approach)
**Purpose**: Broker plugin for Zorro trading platform connecting to cTrader OpenAPI
**Date**: October 8, 2025

## Related Files
The main repository contains additional build files, documentation, and configuration:
- Root folder: Build configuration, solution files
- `.github/workflows/`: CI/CD workflows
- Documentation: `jelentes.md`, `README.md`
