# LockOnTarget

Fast and efficient plugin for Unreal Engine which gives the ability to lock onto a Target similar to Souls-like games. The system is divided into 2 components.

* [**LockOnTargetComponent**](https://github.com/J1blCblu/LockOnTarget/wiki/2.-LockOnTargetComponent-Overview) - gives a locally controlled AActor the ability to capture a Target along with a Socket. The Target is controlled through an optional [TargetHandler](https://github.com/J1blCblu/LockOnTarget/wiki/2.2-Weighted-Target-Handler). The component may contain a set of optional **Extensions** that can be used to add custom cosmetic features such as Target indication.

* [**TargetComponent**](https://github.com/J1blCblu/LockOnTarget/wiki/3.-TargetComponent-Overview) - Represents a Target that *LockOnTargetComponent* can capture in conjunction with a Socket. It is kind of a dumping ground for anything LockOnTarget subsystems may need.


# Features

* Capture *any Actor* with a TargetComponent with multiple sockets.
* Network synchronization.
* Flexible input processing settings.
* Target filtering.
* Auto Target finding in response to events.
* Target switching in screen space.
* [Debugger](https://github.com/J1blCblu/LockOnTarget/wiki/4.-Gameplay-Debugger-Overview).
* Per Target widget customization.


# Installation

* **Source** - Clone the [repository](https://github.com/J1blCblu/LockOnTarget) to the `Plugins` folder of the project.
Optionally add the **LockOnTarget** dependency to your build.cs file. Generate project files and build the project.

* **Unreal Marketplace** - Download from the [Marketplace](https://www.unrealengine.com/marketplace/en-US/product/lock-on-target) and `install` on a specific Engine version. `Enable` the plugin in the editor.


# Documentation

* [Initial setup](https://github.com/J1blCblu/LockOnTarget/wiki/1.-Initial-Setup).
* [Updates](https://github.com/J1blCblu/LockOnTarget/releases).


# Known Issues

List of known [Issues](https://github.com/J1blCblu/LockOnTarget/issues).


# Special Thanks

 * To [mklabs](https://github.com/mklabs). His great [Targeting System](https://github.com/mklabs/ue4-targetsystemplugin) plugin was the starting point.


# License

Source code of the plugin is licensed under MIT license, and other developers are encouraged to fork the repository, open issues & pull requests to help the development.
