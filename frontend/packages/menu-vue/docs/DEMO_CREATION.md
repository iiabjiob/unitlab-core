# Creating Visual Demos (GIF/Video)

This guide explains how to create the visual demos shown in the README.

## Tools Needed

- **Screen Recorder:**
  - macOS: QuickTime Player or [Kap](https://getkap.co/)
  - Windows: [ScreenToGif](https://www.screentogif.com/)
  - Linux: [Peek](https://github.com/phw/peek)
  - Cross-platform: [Gifski](https://gif.ski/)

- **Video to GIF Converter:**
  - [FFmpeg](https://ffmpeg.org/) (command-line)
  - [ezgif.com](https://ezgif.com/) (online)
  - [Gifski](https://gif.ski/) (best quality)

## Demo 1: Basic Menu (`menu-basic.gif`)

**What to show:**
1. Hover over trigger button
2. Click to open menu
3. Hover over menu items (show highlight)
4. Click an item
5. Menu closes

**Settings:**
- Duration: ~3-5 seconds
- FPS: 15-20
- Size: 600x400px
- Loop: Yes

**Recording Steps:**
```bash
# 1. Start your demo app
npm run dev

# 2. Navigate to basic menu example
# 3. Start recording
# 4. Perform actions slowly and clearly
# 5. Stop recording

# 6. Convert to GIF with FFmpeg
ffmpeg -i recording.mov \
  -vf "fps=20,scale=600:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" \
  -loop 0 \
  menu-basic.gif

# 7. Optimize size
gifsicle -O3 --colors 128 menu-basic.gif -o menu-basic.gif
```

## Demo 2: Nested Submenus (`submenu.gif`)

**What to show:**
1. Open main menu
2. Hover over item with submenu indicator (→)
3. Submenu opens to the side
4. Hover over nested submenu trigger
5. Third-level submenu opens
6. Navigate back through levels

**Settings:**
- Duration: ~5-7 seconds
- FPS: 20
- Size: 800x500px
- Loop: Yes

**Pro Tips:**
- Move mouse slowly and deliberately
- Pause briefly when submenus open
- Show at least 3 nesting levels

## Demo 3: Mouse Prediction (`mouse-prediction.gif`)

**What to show:**
1. Open menu with submenu
2. Hover over submenu trigger
3. Move mouse diagonally toward submenu (passing over other items)
4. Show that submenu stays open (key feature!)
5. Successfully reach submenu and hover items

**Settings:**
- Duration: ~4-6 seconds
- FPS: 30 (higher for smooth cursor movement)
- Size: 600x400px
- Loop: Yes

**Critical:**
- Use a cursor highlight tool to make cursor visible
- Move cursor in clear diagonal path
- Briefly hover over "skipped" items to show they don't trigger

**macOS cursor highlighting:**
```bash
# Enable cursor highlight in System Preferences
# Or use MouseCircle app
```

**Windows cursor highlighting:**
```bash
# Use PointerFocus or similar tool
```

## Optimization

### Size Reduction

```bash
# Reduce colors (lossy but smaller)
gifsicle -O3 --colors 64 input.gif -o output.gif

# Reduce frame rate
gifsicle --delay=10 input.gif -o output.gif  # 10 = 100ms = 10fps

# Reduce dimensions
ffmpeg -i input.gif -vf scale=400:-1 output.gif
```

### Best Practices

1. **Keep file size under 2MB** for GitHub README
2. **Use 15-20 FPS** for most demos (30 FPS for cursor demos)
3. **Optimize color palette** to 64-128 colors
4. **Loop seamlessly** - start and end in same state
5. **Add subtle delay** at loop point (200-300ms)

## Alternative: Video Embedding

If GIFs are too large, use video instead:

```markdown
https://user-images.githubusercontent.com/YOUR_ID/video.mp4
```

Or use services like:
- [Imgur](https://imgur.com/) for GIFs
- [Streamable](https://streamable.com/) for videos
- [Cloudinary](https://cloudinary.com/) for optimized media

## Checklist

- [ ] Basic menu demo recorded
- [ ] Submenu demo recorded
- [ ] Mouse prediction demo recorded
- [ ] All GIFs optimized (< 2MB each)
- [ ] GIFs loop smoothly
- [ ] Cursor visible in all demos
- [ ] Placed in `docs/assets/` directory
- [ ] README updated with correct paths
- [ ] Tested on GitHub (push to see rendering)

## Example Recording Script

```bash
#!/bin/bash
# record-demos.sh

echo "Starting demo recordings..."

# Demo 1: Basic Menu
echo "Record basic menu demo - Press Enter when ready"
read
kap --no-sound --duration=5 --out=menu-basic.mp4

# Demo 2: Submenus
echo "Record submenu demo - Press Enter when ready"
read
kap --no-sound --duration=7 --out=submenu.mp4

# Demo 3: Mouse Prediction
echo "Record mouse prediction demo - Press Enter when ready"
read
kap --no-sound --duration=6 --out=mouse-prediction.mp4

# Convert all to GIF
for video in *.mp4; do
  name="${video%.mp4}"
  ffmpeg -i "$video" \
    -vf "fps=20,scale=600:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" \
    -loop 0 \
    "$name.gif"
  
  # Optimize
  gifsicle -O3 --colors 128 "$name.gif" -o "$name.gif"
done

echo "Done! GIFs created in current directory"
```

## Resources

- [How to create high-quality GIFs](https://github.blog/2021-06-22-framework-building-open-graph-images/)
- [GIF optimization guide](https://web.dev/efficient-animated-content/)
- [FFmpeg GIF generation](https://engineering.giphy.com/how-to-make-gifs-with-ffmpeg/)
