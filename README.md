# LockOnTarget

Fast and efficient plugin for Unreal Engine which gives the ability to find and capture the Target, synchronizing it with the server. The system consist of 2 components which can be extended and customized.

* [**LockOnTargetComponent**](https://github.com/J1blCblu/LockOnTarget/wiki/2.-LockOnTargetComponent-Overview) - gives the locally controlled AActor the ability to find and store the Target along with the Socket. The Target can be controlled directly by the component or through an optional [TargetHandler](https://github.com/J1blCblu/LockOnTarget/wiki/2.2-Third-Person-Target-Handler). The component may contain a set of optional **Modules**, which can be used to add custom cosmetic features.

* [**TargetComponent**](https://github.com/J1blCblu/LockOnTarget/wiki/3.-TargetComponent-Overview) - Represents a Target that *LockOnTargetComponent* can capture in conjunction with a Socket. It is kind of a dumping ground for anything LockOnTarget subsystems may need.


# Features

* Capture *any Actor* with a TargetComponent, which can be removed/added at runtime.
* Target can have multiple Sockets, which can be added/removed at runtime.
* Network synchronization.
* Custom Widget for each Target.
* Flexible input processing settings.
* Several useful methods and delegates.
* Target sorting to find the best one.
* Line of Sight check.
* Distance check.
* Auto find a new Target.
* Target switching.
* [Debugger](https://github.com/J1blCblu/LockOnTarget/wiki/4.-Gameplay-Debugger-Overview).


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
