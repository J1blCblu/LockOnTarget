# LockOnTarget
Fast and Efficient plugin for Unreal Engine which gives the ability to capture the Target and follow it.  
The system consist of 2 components which can be extendent and customized in different ways via C++/BP.

### **LockOnTargetComponent**
LockOnTargetComponent gives the Owner(Player) an ability to find a Target and store it. Wraps next systems:
 *	Target storage (*TargetingHelperComponent* and *Socket*). Synchronized with the server.
 *	Processing the input. Processed locally.
 *	**TargetHandler** subobject - used for Target handling (find, switch, maintenance). Processed locally.
 *  **Modules** - piece of functionality that can be dynamically added/removed.

### **TargetingHelperComponent**
Represents the Target that the *LockOnTargetComponent* can capture in addition to a specified *Socket*. Has all component's benefits and acts like a Target specific storage.

# Features

 * Capture *any Actor* with a *TargetingHelperComponent* which can be removed and added at runtime.
 * Target can have **multiple** sockets, which can be added/removed at runtime.
 * Network synchronization.
 * Line of Sight handling.
 * Custom *Widget* for each Target.
 * Switch Targets in any direction (in screen space).
 * Flexible input processing settings.
 * Multiple settings for Target capturing.
 * Various **rotation modes**.
 * Several useful methods and delegates for LockOnTargetComponent and TargetingHelperComponent Owners.
 * Custom rules for capturing the target (e.g. not capturing teammates).
 * Built-in modules by default (CameraZoom, Widget, Control/OwnerRotation).

### **Network Design Philosophy**
**Lock On Target** is just a system which finds a Target, stores and synchronizes it over the network. Finding the Target is not a quick operation to process it on the server for each player. Due to this and network relevancy opportunities (not relevant Target doesn't exist in the world) finding the Target processed **locally on the owning client.**  

# Documentation
[Instructions](https://github.com/J1blCblu/LockOnTarget/wiki) for setting up the plugin.

# Known Issues
List of known [Issues](https://github.com/J1blCblu/LockOnTarget/issues).

# Special Thanks
 * To [mklabs](https://github.com/mklabs). His delightful [Targeting System](https://github.com/mklabs/ue4-targetsystemplugin) plugin was the starting point.

# License
Source code of the plugin is licensed under MIT license, and other developers are encouraged to fork the repository,  \
open issues & pull requests to help the development.
