# UiMenu — Headless Context Menu, Dropdown & SubMenu for Vue 3

A lightweight, **fully headless**, **accessible**, and **positioning-aware** menu system for Vue 3.
Includes:

- Dropdown menus  
- Right-click context menus  
- Multi-level SubMenus  
- Smart positioning + collision handling  
- Amazon‑style mouse prediction  
- Full keyboard control  
- Renderless, style‑agnostic design  

Perfect for dashboards, engineering tools, IDE-like UIs, and professional design systems.

---

# 🚀 Features

- ✔ Headless — bring your own styles  
- ✔ Trigger or cursor anchoring  
- ✔ Collision‑aware positioning  
- ✔ SubMenu hover‑intent prediction (Amazon-style)  
- ✔ Auto‑reposition + ResizeObserver  
- ✔ No memory leaks  
- ✔ Full keyboard accessibility  
- ✔ ARIA roles for screen readers  
- ✔ Teleport to `<body>`  

---

# 📦 Installation

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
} from "@/components/ui/menu"
```

---

# 🧱 Components Overview

| Component | Purpose |
|----------|---------|
| **UiMenu** | Root state provider |
| **UiMenuTrigger** | Opens menu from click / keyboard / right-click |
| **UiMenuContent** | Floating teleported menu |
| **UiMenuItem** | Actionable item |
| **UiMenuLabel** | Section header |
| **UiMenuSeparator** | Divider |
| **UiSubMenu** | Submenu provider |
| **UiSubMenuTrigger** | Opens nested menu |
| **UiSubMenuContent** | Floating nested content |

---

# 🧰 Basic Usage

## Dropdown Menu

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

# 🖱 Context Menu (Right‑Click)

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

# 🎮 Keyboard Interaction

## On Trigger

| Key | Action |
|-----|--------|
| **Enter / Space** | Toggle menu |
| **ArrowDown** | Open + focus first item |
| **Right‑click** | Open context menu |

## Inside Menu

| Key | Action |
|-----|--------|
| **ArrowUp / ArrowDown** | Navigate |
| **Home / End** | Jump to first/last |
| **Enter / Space** | Select item |
| **Esc** | Close and return focus |
| **Tab** | Close and return focus |

---

# 🧠 Positioning Model

Anchor types:

### **1. Trigger anchor**
Opens under the trigger; flips upward when needed.

### **2. Cursor anchor**
Positions at cursor (context menus).

### Repositions on:

- window resize  
- scroll  
- trigger resize  
- menu resize  

Powered by ResizeObserver + global listeners.

---

# 🧩 SubMenu — Multi-level Menus

`UiSubMenu` enables nested menus with UX similar to Radix UI, VSCode, and macOS Finder.

### ✨ Features

- Hover‑intent submenu opening  
- Amazon‑style mouse‑trajectory prediction  
- ArrowRight = open  
- ArrowLeft = close  
- Smooth focus transfer  
- Safe closing delays  
- Viewport‑aware positioning  
- Unlimited depth  

---

# ⚡ Example — Nested SubMenu

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

# 🧠 SubMenu Mouse Behavior (Amazon‑Style Prediction)

To prevent accidental closing while moving diagonally toward submenu:

- Tracks last 2–3 cursor points  
- Computes motion vector  
- Builds a virtual triangle from item → submenu panel  
- If cursor is inside predicted zone → submenu stays open  
- Adds closing delay (150ms)  
- Safe zone: vertical tolerance ±40px  

This achieves **industry‑best submenu UX**.

---

# 🎮 SubMenu Keyboard Interaction

Inside a submenu:

| Key | Action |
|------|--------|
| **→ ArrowRight** | Open submenu |
| **← ArrowLeft** | Close submenu & return focus |
| **↑ / ↓** | Navigate items |
| **Enter / Space** | Activate |
| **Esc** | Close submenu |

---

# 🧰 SubMenu Component API

## `<UiSubMenu>`
Provides submenu state.

### Props  
_None_

---

## `<UiSubMenuTrigger>`
Opens submenu via:

- Hover  
- ArrowRight  
- Enter / Space  

---

## `<UiSubMenuContent>`
Teleported floating content.

Handles:

- Focus navigation  
- Safe close  
- Collision constraints  

---

# 🎨 Styling

The entire system is headless.

Example:

```html
<UiMenuItem class="px-3 py-2 hover:bg-neutral-100 dark:hover:bg-neutral-700" />
```

You may use Tailwind, UnoCSS, SCSS, or plain CSS.

---

# 💎 Full Example (Dropdown + SubMenu)

```vue
<UiMenu>
  <UiMenuTrigger>
    <button class="px-3 py-2 rounded bg-neutral-200">Menu</button>
  </UiMenuTrigger>

  <UiMenuContent>
    <UiMenuLabel>General</UiMenuLabel>

    <UiMenuItem @select="openProfile">Profile</UiMenuItem>
    <UiMenuItem @select="settings">Settings</UiMenuItem>

    <UiSubMenu>
      <UiSubMenuTrigger>More</UiSubMenuTrigger>
      <UiSubMenuContent>
        <UiMenuItem @select="itemA">Item A</UiMenuItem>
        <UiMenuItem @select="itemB">Item B</UiMenuItem>
      </UiSubMenuContent>
    </UiSubMenu>

    <UiMenuSeparator />

    <UiMenuLabel>Danger zone</UiMenuLabel>
    <UiMenuItem danger @select="logout">Log out</UiMenuItem>
  </UiMenuContent>
</UiMenu>
```

---

# 📄 License

MIT — free for personal and commercial use.
