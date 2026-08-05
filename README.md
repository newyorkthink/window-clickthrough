# window-clickthrough

一个用于 X11 的轻量窗口鼠标穿透工具，适合 i3wm、Openbox、Xfce 等 X11 桌面环境。

它只修改所选窗口的 **X11 输入区域**：窗口仍然显示和刷新，但鼠标点击、滚轮会落到后面的窗口。它不会修改窗口透明度、位置、大小或置顶状态。

## 使用效果

第一次运行：

```text
鼠标变成十字 → 左键点击任意窗口 → 开启鼠标穿透
```

再次运行：

```text
自动恢复刚才的窗口，不需要再次选择
```

选择窗口时按鼠标右键可以取消。

## 下载与运行

从 Releases 下载：

```text
window-clickthrough-1.0.0-x86_64.AppImage
```

赋予执行权限：

```bash
chmod +x window-clickthrough-1.0.0-x86_64.AppImage
```

运行：

```bash
./window-clickthrough-1.0.0-x86_64.AppImage
```

不依赖后台服务，不需要 root 权限。

## Kando 配置

Kando 的“执行命令”填写 AppImage 的绝对路径，例如：

```bash
/home/user/Applications/window-clickthrough-1.0.0-x86_64.AppImage
```

项目名称可写：

```text
窗口穿透 / 恢复
```

## 命令行参数

```bash
# 切换：没有活动窗口时选择并开启；已有活动窗口时恢复
./window-clickthrough-1.0.0-x86_64.AppImage

# 强制恢复当前穿透窗口
./window-clickthrough-1.0.0-x86_64.AppImage --restore

# 查看状态；活动时退出码为 0，未活动时退出码为 1
./window-clickthrough-1.0.0-x86_64.AppImage --status

# 查看帮助和版本
./window-clickthrough-1.0.0-x86_64.AppImage --help
./window-clickthrough-1.0.0-x86_64.AppImage --version
```

## 工作方式与安全处理

- 使用 X11 SHAPE 扩展的 `ShapeInput` 区域实现鼠标穿透。
- 同时处理应用窗口和窗口管理器创建的直接父级边框窗口。
- 每次只管理一个穿透窗口。
- 状态文件保存在当前用户的 `XDG_RUNTIME_DIR`；不可用时回退到仅当前用户可访问的 `/tmp` 目录。
- 在目标窗口写入随机令牌；窗口关闭或 X11 窗口 ID 被复用后，不会恢复到无关窗口。
- 使用文件锁阻止多个实例同时修改状态。
- 恢复时只恢复鼠标输入区域，不修改其他窗口属性。

## 限制

- 仅支持 X11，不支持原生 Wayland 窗口。
- 开启穿透后无法直接点击该窗口，必须再次运行本工具或使用 `--restore`。
- 恢复操作将输入区域恢复为窗口默认矩形；原本使用自定义输入形状的特殊窗口不建议使用。
- 当前发布只构建 x86_64 AppImage。

## 本地构建

Debian/Ubuntu：

```bash
sudo apt-get install build-essential libx11-dev libxext-dev xvfb xdotool x11-utils
make check
scripts/build-appimage.sh
```

Arch Linux：

```bash
sudo pacman -S --needed base-devel libx11 libxext xorg-server-xvfb xdotool xorg-xdpyinfo
make check
scripts/build-appimage.sh
```

生成文件位于：

```text
dist/window-clickthrough-1.0.0-x86_64.AppImage
```

## GitHub Actions

工作流只在以下情况构建：

- `main` 中与源码、测试、打包或工作流有关的文件发生变化。
- 手动运行 `Build AppImage`。

文档单独修改不会触发构建。构建过程会先执行编译警告检查和 Xvfb 集成测试，再生成 AppImage、SHA-256 校验文件和 Release。
