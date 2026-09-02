# Build Requirements
- GCC
- NASM
- Xorriso
- Grub
- Grub EFI
- Cmake
- Makefiles
- MTools
- Dosfs Tools

### apt:
```bash
sudo apt update
sudo apt install -y build-essential nasm xorriso grub-pc-bin grub-common qemu-system-x86 cmake gdb grub-efi-amd64-bin mtools dosfstools
```
### dnf:
```bash
sudo dnf update
sudo dnf install @development-tools
sudo dnf install -y nasm xorriso grub2-pc grub2-tools grub2-tools-extra qemu-system-x86 cmake gdb grub2-efi-x64 mtools dosfstools
```
