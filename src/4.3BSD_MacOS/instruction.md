# 4.3BSD on SIMH VAX-11/780 (macOS Setup)

This guide explains how to set up and run the 4.3BSD (University of Wisconsin) machine on macOS. Since this simulator is an older Intel-based application, it requires Intel (x86_64) libraries to run on modern Apple Silicon Macs (M1, M2, M3).

## Install Intel Homebrew (Prerequisite)

Because the simulator is built for Intel chips, you cannot use the standard Apple Silicon Homebrew located in `/opt/homebrew`. You must install a separate Intel version of Homebrew in `/usr/local`.

### Steps

1. Open Terminal.
2. Install Rosetta 2 (required to run Intel applications):

```bash
softwareupdate --install-rosetta --agree-to-license
```

3. Install Intel Homebrew into `/usr/local`:

```bash
arch -x86_64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

## Install Required Libraries

Once Intel Homebrew is installed, use it to install the required dependencies.

Run the following command exactly as written:

```bash
arch -x86_64 /usr/local/bin/brew install vde sdl2 sdl2_ttf libpcap
```

> Note: Using `arch -x86_64` ensures you are using the Intel version of Homebrew, not the Apple Silicon one.

## Starting the Simulator

You must run the simulator from inside its directory so it can find its configuration files.

1. Open Terminal.
2. Navigate to the folder where you extracted the VAX files (you can type `cd` and then drag the folder into the terminal window):

```bash
cd ~/Downloads/path/to/vax780_folder
```

3. Start the simulator:

```bash
./vax780
```

4. Initialize the System: When you see the sim> prompt, type the following command to load the configuration:

```bash
do vax780_old.ini
```

If successful, you will see SIMH version information and boot messages.

## Connecting to the Console

## Listening to Port 40316

The simulator often detaches the console to a network port instead of accepting input directly in the terminal window. The default port is usually `40316`.

1. Leave the simulator running in the first terminal window.
2. Open a new Terminal window (Cmd + N).
3. Connect to the console using `telnet`:

```bash
telnet localhost 40316
```

4. If prompted to login, just use

```bash
root
```

and hit enter

> Note: macOS High Sierra and later do not include `telnet` by default. If `telnet` is not found, install it using Homebrew:

```bash
brew install telnet
```

## Troubleshooting

- **"File open error"**: You ran `./vax780` from the wrong directory. Make sure you `cd` into the folder containing the simulator files first.

- **"Library not loaded" or "wrong architecture"**: You are missing dependencies or installed Apple Silicon libraries instead of Intel ones. Reinstall dependencies using the Intel Homebrew command above.

- **"Connection refused"**: The simulator is not running yet or has not finished booting. Wait a few seconds and try the `telnet` command again.
