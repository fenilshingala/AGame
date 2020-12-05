# AGame

My first attempt to create a cross-platform application!

* Platforms
  * Windows (x64)
  * Android (ARM64)
  
### Current state
* Rendering a Triangle!
  - [x] Vulkan objects: Instance, Device, Sync Objects and Swapchain
  - [x] RenderTarget (renderpass, framebuffers)
  - [x] Shader modules and Graphics pipeline
  - [x] (Command)pool, buffers, and various commands

* Entity Component System
  - [x] Entity and Component creation, deletion
  - [x] Component registration to help Deserialization by component name

- [x] Basic File and Directory Operations
- [x] Log implementation: 3 types (INFO, WARNING, ERR)

### Not working
- [ ] Android triple-buffer
- [ ] Android app safe exit

### To Do
- [ ] Handle app minimize
- [ ] Android validation layer
- [ ] InputManager (Keyboard, Mouse, Virtual Joystick for Android)
- [ ] Shader buffers and texture sampling to support model loading