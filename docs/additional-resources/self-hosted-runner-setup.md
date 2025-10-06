# Setting up a self-hosted runner for Proves Core Reference builds

## Overview

### What is CI/CD?

**CI/CD** stands for **Continuous Integration** and **Continuous Deployment**. Think of it as an automated quality control system for software development:

- **Continuous Integration (CI)**: Automatically builds and tests code every time changes are made
- **Continuous Deployment (CD)**: Automatically deploys tested code to production environments

### What is an automated build system?

An automated build system consists of machines that compile code, run tests, and sometimes deploy software. It automates the process of building software from source code, ensuring consistency and reliability in the build process.

**Here's what happens in our project when you make changes:**

1. **Code changes** are pushed to GitHub (via pull request or direct push)
2. **Automated build** starts - compiles the firmware and F-Prime components
3. **Tests run** - both unit tests and integration tests with real hardware
4. **Results reported** - you see ✅ or ❌ status on our pull request
5. **Build artifacts** are saved (like the `.uf2` firmware file)

This ensures that every change is automatically verified before it gets merged into the main codebase.

### GitHub Actions

GitHub Actions is a CI/CD platform that allows us to automate our software development workflows directly in our GitHub repository. It provides a way to define custom workflows using YAML files, which can include various actions such as building code, running tests, and deploying applications.

**Key concepts:**

- **Workflow**: A automated process defined in a YAML file (like `.github/workflows/ci.yaml`)
- **Job**: A set of steps that run on the same machine (like `build`, `lint`, `integration`)
- **Step**: An individual task within a job (like "checkout code" or "run tests")
- **Runner**: The machine that executes the workflow (GitHub-hosted or self-hosted)

**When workflows run:**

- On every pull request (to test changes before merging)
- When code is pushed to the main branch
- On scheduled intervals (for nightly builds)
- Manually triggered by developers

### Why We Need Self-Hosted Runners

Self-hosted runners are machines that you manage and maintain to run our GitHub Actions workflows. Unlike GitHub's hosted runners, which are provided and maintained by GitHub, self-hosted runners give you more control over the environment in which our workflows run.

We use self-hosted runners because they allow us to run tests on PROVES hardware that is connected to the runner machine. This is important because some tests require access to specific hardware components that are not available on GitHub's hosted runners.

### Understanding Pull Request Status Checks

When you create a pull request, you'll see status checks at the bottom that show the progress of our CI pipeline:

![Example of GitHub status checks](https://docs.github.com/assets/cb-17910/mw-1440/images/help/pull_requests/pr-checks-success.webp)

**Status meanings:**

- **🟡 In Progress**: Tests are currently running
- **✅ Passed**: All tests completed successfully
- **❌ Failed**: One or more tests failed (click "Details" to see why)
- **⏸️ Pending**: Waiting for previous jobs to complete

**Common failure scenarios:**
For all failures, click "Details" next to the failed check to see logs and error messages. It's important to read these messages carefully to understand what went wrong.

- **Lint fails**: Code formatting and linting issues
  - Most likely running `make fmt` locally and committing the result will resolve
- **Build fails**: Compilation errors
  - Run `make build` to troubleshoot
- **Integration fails**: Hardware tests failed
  - Run `make test-integration` locally with the hardware connected to troubleshoot

### Development Workflow

Here's how CI/CD fits into a typical development process:

1. **Create a branch** from `main` for our feature
2. **Make code changes** and commit them
3. **Push to GitHub** - this triggers the build pipeline
4. **Create a pull request** - this triggers the full CI pipeline
5. **Wait for tests** - green checkmarks mean you're good to go
6. **Address any failures** - fix issues and push new commits
7. **Merge when green** - our code is automatically validated before merging

The CI system acts as a safety net, catching problems before they reach the main branch.

## Setting up a self-hosted runner

### Prerequisites

- A machine running a Linux distribution (Ubuntu is recommended)
- A GitHub account with access to the repository where you want to set up the self-hosted runner
- Basic knowledge of Linux command line and GitHub Actions
- Sudo access on the machine

### Steps

1. **Install necessary software**: Ensure that the machine has required software installed.

```bash
sudo apt update
sudo apt install make cmake wget curl build-essential
```

Install picotool (needed for flashing firmware to PROVES hardware):

```bash
curl https://github.com/raspberrypi/pico-sdk-tools/releases/download/v2.2.0-2/picotool-2.2.0-a4-x86_64-lin.tar.gz -o picotool-2.2.0-a4-x86_64-lin.tar.gz
tar -xzf picotool-2.2.0-a4-x86_64-lin.tar.gz
mv picotool-2.2.0-a4-x86_64-lin/picotool/* /usr/local/bin
```

2. **Set permissions for USB access**: To allow the runner to access PROVES hardware connected via USB, we need to set appropriate permissions.

Add user to plugdev and dialout to give spacelab user usb perms

```sh
sudo usermod -a -G plugdev $USER
sudo usermod -a -G dialout $USER
```

Add udev rules to allow picotool to access USB without root

```sh
sudo curl https://raw.githubusercontent.com/raspberrypi/picotool/refs/heads/master/udev/60-picotool.rules -o /etc/udev/rules.d/60-picotool.rules
sudo udevadm control --reload
```

3. **Reboot the machine**: This ensures that all changes take effect, especially group membership changes.

```sh
reboot
```

4. **Download and configure the self-hosted runner**: Follow [GitHub's official documentation](https://github.com/organizations/Open-Source-Space-Foundation/settings/actions/runners/new?arch=x64&os=linux) to download and configure the self-hosted runner for our repository.

   - When configuring the runner in `config.sh`, ensure that when prompted, you enter `proves` as the runner group, and that you add `integration` as a label, otherwise the runner won't fit the criteria to be used for the workflow.

5. **Connect the PROVES hardware**: Ensure that the PROVES hardware is connected to the machine via USB.

6. **Run the self-hosted runner**: Start the runner using the provided script.

```sh
./run.sh
```

Ensure that you set our computer to not sleep or hibernate, as this will stop the runner. Also ensure that the runner is restarted if the machine reboots.
