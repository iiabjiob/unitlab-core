# 📌 UiMenu — Headless Context Menu & Dropdown for Vue 3

A lightweight, **fully headless**, **accessible**, and **positioning-aware** menu system for Vue 3.
Supports:

- Trigger-based dropdown menus
- Right-click context menus
- Keyboard navigation
- Collision-aware positioning (opens above if needed)
- Auto-repositioning via ResizeObserver
- Focus management like RadixUI / HeadlessUI
- Fully customizable UI (renderless logic)

Perfect for dashboards, engineering tools, and scalable design systems.

---

## 🚀 Features

- ✔ Headless — bring your own styles
- ✔ Trigger or cursor anchoring (context menu)
- ✔ Smart, collision-aware positioning
- ✔ Auto-reposition + ResizeObserver
- ✔ No memory leaks (stable global listeners)
- ✔ Full keyboard accessibility
- ✔ Correct ARIA roles
- ✔ Teleport to `<body>` to avoid clipping

---

## 📦 Installation

Import directly:

```ts
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuLabel,
  UiMenuSeparator
} from "@/components/ui/menu"
```

---

## 🧱 Components Overview

| Component | Purpose |
|----------|---------|
| **UiMenu** | Root provider managing state & positioning |
| **UiMenuTrigger** | Activates menu on click, Enter, Space, ArrowDown, or right-click |
| **UiMenuContent** | Floating (teleported) menu container |
| **UiMenuItem** | Actionable item with keyboard + mouse activation |
| **UiMenuLabel** | Non-interactive label |
| **UiMenuSeparator** | Horizontal divider |

---

## 🧰 Basic Usage

### Dropdown (left-click)

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

### Context Menu (right-click)

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

### **On trigger**
| Key | Action |
|-----|--------|
| Enter / Space | Toggle menu |
| ArrowDown | Open + focus first item |
| Right-click | Open at cursor |

### **Inside menu**
| Key | Action |
|-----|--------|
| ArrowUp / ArrowDown | Navigate |
| Home / End | Jump to first/last |
| Enter / Space | Activate item |
| Esc | Close + return focus to trigger |
| Tab | Close + return focus to trigger |

---

## 🧠 Positioning Model

Supports two anchor modes:

### **1. Trigger anchor**
Opens under the trigger unless there is no space — then opens above.

### **2. Cursor anchor**
Uses `MouseEvent.clientX` / `clientY`, clamped to viewport.

### Auto-repositioning triggers:

- Resize
- Scroll
- Trigger resize
- Content resize

Handled via global listeners + ResizeObserver.

---

## ⚙ Events

### `@open`
Fires when the menu opens.

### `@close`
Fires when the menu closes.

### `UiMenuItem @select`
Fires when item activates (click / Enter / Space).

```vue
<UiMenuItem @select="doSomething">Do something</UiMenuItem>
```

---

## 🎨 Styling

Completely headless — style things as needed:

```html
<UiMenuItem class="px-3 py-2 hover:bg-neutral-100 dark:hover:bg-neutral-700" />
```

---

## 💎 Example: Full Menu

```vue
<UiMenu>
  <UiMenuTrigger>
    <button class="px-3 py-2 rounded bg-neutral-200">Menu</button>
  </UiMenuTrigger>

  <UiMenuContent>
    <UiMenuLabel>General</UiMenuLabel>

    <UiMenuItem @select="openProfile">Profile</UiMenuItem>
    <UiMenuItem @select="settings">Settings</UiMenuItem>

    <UiMenuSeparator />

    <UiMenuLabel>Danger zone</UiMenuLabel>

    <UiMenuItem danger @select="logout">Log out</UiMenuItem>
  </UiMenuContent>
</UiMenu>
```

---

## 📄 License

MIT — free for personal and commercial use.
