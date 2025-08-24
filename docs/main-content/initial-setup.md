# Initial Project Setup

This guide will walk through the steps of setting up the reference deployment.

<!-- TODO: UPDATE REPO NAME -->
## 1. Clone the GitHub repository
Clone the GitHub repository onto your local machine.
```sh
git clone https://github.com/fprime-community/fprime-zephyr-reference.git
```

> [!NOTE]
> If you would like to use fprime-bootstrap instead of git to clone this project, run this command and skip to step 5.
> ```sh
> fprime-bootstrap clone https://github.com/fprime-community/fprime-zephyr-reference.git
> ```

## 2. Fetch git submodules
Install the required libraries for this deployment
```sh
# In fprime-zephyr-reference
git submodule update --recursive --init
```

## 3. Create a virtual environment
Create a virtual environment in the main project directory

```sh
# In fprime-zephyr-reference
python3 -m venv fprime-venv
```

## 4. Activate the virtual environment

```sh
# In fprime-zephyr-reference
# Linux, MacOS, & Windows WSL
source fprime-venv/bin/activate
```

## 5. Install python requirements
With the virtual environment activated, install the requirements
```sh
# In fprime-zephyr-reference (fprime-venv)
pip install -r requirements.txt
```

 > [!NOTE]
 > You will need to follow some additional steps if you are using a different board than the one specified in the `settings.ini` file. Please refer to the [Specifying Board Configuration][specifying-board-configuration] documentation for more information.


## 6. Get Zephyr Source Code
Navigate to the `zephyr-workspace` directory to set up zephyr. You may need to update the `config` file in `./lib/zephyr-workspace/.west/` if you are using a different board. Refer to the documentation [here][specifying-board-configuration]
```sh
# In fprime-zephyr-reference
cd lib/zephyr-workspace

# Run the following commands
west update
west zephyr-export
```

> [!NOTE]
> If you have not installed the [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html#toolchain-zephyr-sdk), please install it with the following command:
> ```shell
> # In fprime-zephyr-reference/lib/zephyr-workspace/zephyr
> west sdk install
> ```
> The Zephyr SDK only needs to be installed once.

# Next Steps: [Building, Flashing, and Running the Deployment][build-flash-run]

<!-- Links -->
[build-flash-run]: ./build-flash-run.md
[specifying-board-configuration]: ../additional-resources/specifying-board-configuration.md
