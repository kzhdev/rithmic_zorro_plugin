# Rithmic Zorro Plugin

This project is a plugin for the **[Zorro](https://zorro-project.com/)** Automated Trading System, integrating with Rithmic's **[R | API+](https://www.rithmic.com/apis)** to provide market data and trading capabilities.

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Development](#development)
- [License](#license)

## Installation

To install the plugin, follow these steps:

1. Download the latest release from the [releases page](https://github.com/kzhdev/rithmic_zorro_plugin/releases).
2. Unzip the downloaded file.
3. Copy the `rithmic.dll` from rithmic_zorro_plugin.zip into the `plugin` folder under Zorro's root path.
4. Copy the `rithmic.dll` from rithmic_zorro_plugin64.zip into `plugin64` folder under Zorro's root path.
5. Copy the `rithmic_ssl_cert_auth_params and rithmic_config.toml` into Zorro's root path.

## Usage

To use the plugin, follow these steps:

1. Open Zorro, select Rithmic as the broker.
3. Enter the **Your Rithmic User ID** in the **User ID** input box.
4. Enter the **Your Rithmic Password** in the **Password** input box.

## Configuration

### Plugin Configuration
The plugin is configured via the `Zorro.ini` or `ZorroFix.ini` file (preferred). Bellow are the plugin specific configurations:

```ini
RithmicLogLevel=2     // Optional. 0=TRACE, 1=DEBUG, 2=INFO, 3=WARNING, 4=ERROR, 5=CRITICAL, 6=OFF Default to 2(INFO).
RithmicConfigPath="path of the rithmic config toml file"    // Optional. Default to "rithmic_config.toml" in Zorro's root folder
```

**RithmicLogLevel**: Sets the plugin's logging level. Default to INFO (2).

**RithmicConfigPath**: Specifies the location of the rithmic_config.toml file. Use the full path if not in Zorro's root folder.

### Rithimc Server Configuration
The `rithmic_config.toml` file, included in the release, defines Rithimic server settings for environments like Rithmic 01, Rithimic Paper Trading and Rithmic Test.
User can obtain server configuration details from their broker or extract them from the Rithmic Trader Pro log file located at Documents/Rithmic/ (e.g. Rithmic Trader Pro.cur.txt).
Open the log file in a text editor to find values for server addresses and other parameters.

The `rithmic_config.toml` file uses the following format:
```toml
["Rithmic Server 1"]
MML_DMN_SRVR_ADDR = "..."
MML_DOMAIN_NAME = "..."
MML_LIC_SRVR_ADDR = "..."
MML_LOC_BROK_ADDR = "...."
MML_LOGGER_ADDR = "..."
MML_LOGGER_TYPE = "..."
MML_SSL_CLNT_AUTH_FILE = "..."

["Rithmic Server 2"]
MML_DMN_SRVR_ADDR = "..."
MML_DOMAIN_NAME = "..."
MML_LIC_SRVR_ADDR = "..."
MML_LOC_BROK_ADDR = "...."
MML_LOGGER_ADDR = "..."
MML_LOGGER_TYPE = "..."
MML_SSL_CLNT_AUTH_FILE = "..."

......

```
#### Fields:
* **MML_DMN_SRVR_ADDR**: Market data server address.
* **MML_DOMAIN_NAME**: Rithmic domain name.
* **MML_LIC_SRVR_ADDR**: License server address.
* **MML_LOC_BROK_ADDR**: Local Broker server address.
* **MML_LOGGER_ADDR**: Logging server address.
* **MML_LOG_TYPE**: Logging type. (Optional) Default to log_net.
* **MML_SSL_CLNT_AUTH_FILE**: Path to the `rithmic_ssl_cert_auth_params` file. Use the full path if not in Zorro's root folder. (Optional) Default to `rithmic_ssl_cert_auth_params`.

## Assets.csv

In the assets.csv, there must be a `Symbol` column which has `.<exchange>` at the end of an asset. For example:
```csv
Name,Price,Spread,RollLong,RollShort,PIP,PIPCost,MarginCost,Market,Multiplier,Commission,Symbol
ESH5,5993.75,0.25,0.0,0.0,0.25000,0.0100000,-98.7,0,1.00,0.0,*.CME
```

## Supported Broker Commands
Following Zorro Broker API functions have been implemented:
- BrokerOpen
- BrokerLogin
- BrokerAsset
    - Only support Balance
- BrokerHistory2
- BrokerBuy2
- BrokerTrade
    - Only output pOpen, the average fill price. 
- BrokerCommand
    - GET_COMPLIANCE
    - GET_BROKERZONE
    - GET_MAXTICKS
    - GET_POSITION
        ```c++
        // The Symbol needs to be in <Asset>.<Exchange> format
        brokerCommand(GET_POSITION, "ESH5.CME");
        ```
    - GET_AVGENTRY
    - SET_ORDERTEXT
    - SET_SYMBOL
    - SET_ORDERTYPE (0:IOC, 1:FOK, 2:GTC)
    - SET_WAIT
    - GET_PRICETYPE
    - SET_PRICETYPE
    - SET_AMOUNT
    - SET_DIAGNOSTICS
    - SET_LIMIT
    - DO_CANCEL
    - GET_VOLTYPE

    Plugin specific commands:
    - 2000: Set logging level
        ```c++
        brokerCommand(2000, loging_level);
        // Valid logging levels are:
        // 0: TRACE
        // 1: DEBUG
        // 2: INFO
        // 3: WARNING
        // 4: ERROR
        // 5: CRITICAL
        // 6: OFF
        ```
    - 2001: Override default order type to DAY. This command has no effect if GTC order is used.
        ```c++
        brokerCommand(2001, 1 or 0);
        // 1: Use Day order type
        // 0: Use default order type, IOC
        ```

## Development

### Prerequisites

- CMake 3.16 or higher
- Visual Studio
- Rithmic API SDK
- Zorro Trading Platform

### Installing Dependencies

Rithimic Zorro Plugin relies on several external libraries. You can install these dependencies using [vcpkg](https://github.com/microsoft/vcpkg). Follow these steps to install the required libraries:

1. **Install vcpkg**:
    ```sh
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ```

2. **Install Required Libraries**:
    ```sh
    ./vcpkg install spdlog
    ./vcpkg install date
    ```

3. **Integrate vcpkg with CMake**: 
    Add the following line to your CMake configuration to use vcpkg:
    ```sh
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
    ```

### Building the Plugin

1. Clone the repository:
    ```sh
    git clone https://github.com/yourusername/rithmic_zorro_plugin.git
    cd rithmic_zorro_plugin
    ```

2. Set the environment variables for the Rithmic API SDK:
    ```sh
    set RITHMIC_API_INCLUDE_DIR=path/to/rithmic/include
    set RITHMIC_API_LIB_DIR=path/to/rithmic/lib
    ```
    If using VSCode, these variables can also be set in setting.json under .vscode folder
    ```ini
    "cmake.configureEnvironment": {
        "RITHMIC_API_INCLUDE_DIR": "<path to RApiPlus include directory>",
        "RITHMIC_API_LIB_DIR": "<path to RApiPlus lib directory>",
        "ZORRO_ROOT": "<Path to Zorro root directory>"
    }
    ```
3. Run CMake:
    ```sh
    cmake -S . -B ./build
    ```

4. Build the project:
    ```sh
    cmake --build ./build --config Release
    ```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on [GitHub](https://github.com/kzhdev/rithmic_zorro_plugin/issues).

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
