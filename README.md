# AGame

My first attempt to create a cross-platform application!

* Platforms
  * Windows (x64)
  * Android (ARM64)
  
### Current state
* Renderer
  - [x] Vulkan objects: Instance, Device, Sync Objects and Swapchain
  - [x] RenderTarget (renderpass, framebuffers)
  - [x] Shader modules and Graphics pipeline
  - [x] (Command)pool, buffers, and various commands
  - [x] Descriptor management (Create, Update and Bind descriptor sets)
  - [x] Shader buffers and texture sampling to support model loading

* Entity Component System
  - [x] Entity and Component creation, deletion
  - [x] Component registration to help Deserialization by component name

- [x] Basic File and Directory Operations
- [x] Log implementation: 3 types (INFO, WARNING, ERR)

### To Do
- [ ] Depth buffering
- [ ] Model loading
- [ ] Mipmaps
- [ ] Multisampling
- [ ] InputManager (Keyboard, Mouse, Virtual Joystick for Android)
- [ ] Open window with custom position, size (Windows OS)
