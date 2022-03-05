# LockOnTarget
Fast and Efficient plugin for Unreal Engine which gives the ability to capture the Target and follow it.  \
System consist of 2 components which can be extendent and customized in different ways via C++/BP.

### **LockOnTargetComponent**
**Main component** which capture `Targeting Helper Component` and wraps the following systems:
  * Target storage (Helper Component and Socket).
  * Processing input.
  * Delegating other work to it subobjects:
    * **TargetHandler** - used for handle Target(find, switch, maintenance).
    * **RotationModes** - return final rotation for Control/Owner Rotation.

### **TargetingHelperComponent**
Captured by `LockOnTarget component`. Target specific storage and useful methods, delegates for it owner.

# Features

 * Targeting to `any AActor subclass`. Adding and removing Helper component from any Actor at runtime.
 * Target can have `multiple sockets`, which can be added/removed at runtime.
 * Line of Sight handling.
 * Flexible processing input settings.
 * Multiple settings for capturing Target.
 * Custom rules for Target finding, switching, maintenance.
 * Various **rotation types**.
 * Custom owner/control rotation rules.
 * Multiple useful methods and delegates for LockOnTargetComponent's and TargetingHelperComponent's Owners.
 * Several customizable **Target offset** types.
 * Add custom rules for capturing the target (e.g. not capturing teammates).
 * Ability to Unlock All Invaders (*LockOnTargetComponent*) or specific one from Target on *request*.
 * Determine the mesh component by name.
 * Implicitly adds UWidgetComponent to Target (use *GetWidgetComponent()* to access it).

# Documentation
[Instructions](https://github.com/J1blCblu/LockOnTarget/wiki) for setting up the plugin.

# Known Issues
List of known [Issues](https://github.com/J1blCblu/LockOnTarget/issues).

# License
Source code of the plugin is licensed under MIT license, and other developers are encouraged to fork the repository,  \
open issues & pull requests to help the development.
