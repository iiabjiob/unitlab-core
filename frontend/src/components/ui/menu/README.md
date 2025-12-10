# UiMenu — Headless Context Menu, Dropdown & SubMenu for Vue 3

A lightweight, **fully headless**, **accessible**, and **positioning‑aware** menu system for Vue 3.

Supports:
- Dropdown menus
- Right‑click context menus
- Multi‑level SubMenus
- Smart collision‑aware positioning
- Amazon‑style mouse prediction
- Full keyboard accessibility
- Teleport to `<body>`
- Theme customization (light/dark/custom)
- Zero dependencies

Perfect for dashboards, engineering tools, IDE-like UIs, and professional design systems.

---

## 🚀 Features

- ✔ Headless — bring your own styles  
- ✔ Trigger or cursor anchoring  
- ✔ Collision‑aware positioning  
- ✔ SubMenu hover‑intent prediction (Amazon-style)  
- ✔ Auto‑reposition via ResizeObserver  
- ✔ No memory leaks  
- ✔ Full keyboard accessibility  
- ✔ ARIA roles for screen readers  
- ✔ Teleport to `<body>`  
- ✔ Fully themable via CSS Variables  

---

## 📦 Installation

```ts
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuLabel,
  UiMenuSeparator,
  UiSubMenu,
  UiSubMenuTrigger,
  UiSubMenuContent
} from "@unitlab/ui-menu"
```

---

## 🧱 Components Overview

| Component | Purpose |
|----------|---------|
| **UiMenu** | Root state provider |
| **UiMenuTrigger** | Opens menu via click / keyboard / right‑click |
| **UiMenuContent** | Floating teleported menu container |
| **UiMenuItem** | Actionable item |
| **UiMenuLabel** | Section heading |
| **UiMenuSeparator** | Divider |
| **UiSubMenu** | Submenu provider |
| **UiSubMenuTrigger** | Opens nested menu |
| **UiSubMenuContent** | Floating nested content |

---

## 🧰 Basic Usage

### Dropdown Menu

```vue
<UiMenu>
  <UiMenuTrigger>
    <button class="btn">Options</button>
  </UiMenuTrigger>

  <UiMenuContent>
    <UiMenuItem @select="edit">Edit</UiMenuItem>
    <UiMenuItem @select="duplicate">Duplicate</UiMenuItem>
    <UiMenuSeparator />
    <UiMenuItem danger @select="remove">Delete</UiMenuItem>
  </UiMenuContent>
</UiMenu>
```

---

## 🖱 Right‑Click Context Menu

```vue
<div @contextmenu.prevent="menuRef.openAtCursor($event)">
  Right-click here
</div>

<UiMenu ref="menuRef">
  <UiMenuContent>
    <UiMenuItem @select="copy">Copy</UiMenuItem>
    <UiMenuItem @select="paste">Paste</UiMenuItem>
  </UiMenuContent>
</UiMenu>
```

---

## 🎮 Keyboard Interaction

### Trigger

| Key | Action |
|-----|--------|
| **Enter / Space** | Toggle menu |
| **ArrowDown** | Open + focus first item |
| **Right‑click** | Open context menu |

### Inside Menu

| Key | Action |
|------|--------|
| **ArrowUp / ArrowDown** | Navigate |
| **Home / End** | Jump to first/last |
| **Enter / Space** | Select |
| **Esc** | Close + return focus |
| **Tab** | Close |

---

## 🧠 Positioning Model

Two anchor types:

### 1. *Trigger anchor*  
Opens below trigger; flips above if needed.

### 2. *Cursor anchor*  
Positions directly at cursor for context menus.

Repositions automatically when:
- window resizes  
- scrolling  
- menu resizes  
- trigger resizes  

Powered by **ResizeObserver**.

---

## 🧩 SubMenu — Multi‑level Menus

`UiSubMenu` supports nested menus with behavior similar to VSCode, RadixUI and macOS Finder.

### Features
- Hover intent detection  
- Amazon‑style mouse trajectory prediction  
- Smooth keyboard navigation  
- Safe close delay  
- Viewport‑aware positioning  
- Unlimited nesting  

### Example

```vue
<UiMenu>
  <UiMenuTrigger>
    <button class="px-3 py-2 bg-neutral-200 rounded">Menu</button>
  </UiMenuTrigger>

  <UiMenuContent>
    <UiMenuItem @select="openFile">Open File</UiMenuItem>

    <UiSubMenu>
      <UiSubMenuTrigger>File Actions</UiSubMenuTrigger>
      <UiSubMenuContent>
        <UiMenuItem @select="rename">Rename</UiMenuItem>
        <UiMenuItem @select="duplicate">Duplicate</UiMenuItem>

        <UiSubMenu>
          <UiSubMenuTrigger>Advanced</UiSubMenuTrigger>
          <UiSubMenuContent>
            <UiMenuItem @select="compress">Compress</UiMenuItem>
            <UiMenuItem @select="archive">Archive</UiMenuItem>
          </UiSubMenuContent>
        </UiSubMenu>
      </UiSubMenuContent>
    </UiSubMenu>

    <UiMenuSeparator />

    <UiMenuItem danger @select="deleteItem">Delete</UiMenuItem>
  </UiMenuContent>
</UiMenu>
```

---

# 🎨 Theming & Dark Mode

`UiMenu` is fully styled via **CSS Variables**, making it compatible with Tailwind, Bootstrap, SCSS, UnoCSS and vanilla CSS.

## Default Theme

```css
:root {
  --ui-menu-bg: #ffffff;
  --ui-menu-text: #1f1f1f;
  --ui-menu-hover-bg: #f3f3f3;
  --ui-menu-border: #dddddd;
  --ui-menu-muted: #6b6b6b;
  --ui-menu-danger: #d32f2f;
  --ui-menu-radius: 8px;
  --ui-menu-item-radius: 6px;
  --ui-menu-shadow: 0 10px 30px rgba(15, 23, 42, 0.12);
}
```

---

## 🌙 Dark Mode (compatible with Tailwind `.dark`)

```css
.dark {
  --ui-menu-bg: #1f1f1f;
  --ui-menu-text: #f3f3f3;
  --ui-menu-hover-bg: #2b2b2b;
  --ui-menu-border: #333333;
  --ui-menu-muted: #999999;
  --ui-menu-danger: #ff6b6b;
}
```

Enable automatically with:

```html
<html class="dark">
```

Or scoped:

```html
<div class="dark">
  <UiMenu />
</div>
```

---

## 🎨 Custom Theme (Tailwind)

```css
@layer base {
  :root {
    --ui-menu-bg: theme('colors.neutral.50');
    --ui-menu-text: theme('colors.neutral.900');
    --ui-menu-hover-bg: theme('colors.neutral.200');
  }

  .dark {
    --ui-menu-bg: theme('colors.neutral.900');
    --ui-menu-text: theme('colors.neutral.100');
    --ui-menu-hover-bg: theme('colors.neutral.700');
  }
}
```

---

## 🎨 Custom Theme (Vanilla CSS)

```css
.my-menu-theme {
  --ui-menu-bg: #242424;
  --ui-menu-text: #eaeaea;
  --ui-menu-hover-bg: #333;
  --ui-menu-border: #444;
}
```

Usage:

```html
<div class="my-menu-theme">
  <UiMenu />
</div>
```

---

# 📚 API Summary

## `<UiMenu>`
Root provider, manages state.

## `<UiMenuTrigger>`
- Click to open
- Right‑click context menu
- Keyboard activation

## `<UiMenuContent>`
Teleported floating container.

## `<UiMenuItem>`
Selectable menu item.

Props:
- `danger?: boolean`

Events:
- `@select`

## `<UiSubMenu>`
Provides nested menu context.

## `<UiSubMenuTrigger>`
Opens submenu on hover / keyboard.

## `<UiSubMenuContent>`
Teleported floating submenu.

---

# 🧪 Accessibility

- Full keyboard control  
- Roving focus  
- ARIA roles (`menu`, `menuitem`, `separator`)  
- Escape & Tab handling  
- Focus return to trigger  

---

# 📄 License

MIT — free for personal and commercial use.
