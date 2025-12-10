# UiMenu

> Headless, accessible menu components for Vue 3 with intelligent positioning and keyboard navigation.

A lightweight, framework-agnostic menu system built for production applications. Provides dropdown menus, context menus, and nested submenus with smart positioning, hover-intent detection, and full keyboard accessibility.

## Features

- **Fully headless** — Complete control over styling and markup
- **Accessible by default** — ARIA roles, keyboard navigation, focus management
- **Smart positioning** — Viewport-aware with automatic collision detection
- **Context menu support** — Trigger from clicks or cursor position
- **Nested submenus** — Unlimited depth with Amazon-style hover prediction
- **Auto-repositioning** — ResizeObserver tracks layout changes
- **Zero dependencies** — Pure Vue 3 with no external libraries
- **Theme-ready** — CSS variables for light/dark modes

Ideal for dashboards, data tools, and applications requiring professional menu UX.

## Installation

This library is designed as a component suite for Vue 3 projects. Import directly from your UI component library:

```typescript
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuLabel,
  UiMenuSeparator,
  UiSubMenu,
  UiSubMenuTrigger,
  UiSubMenuContent,
} from "@/components/ui/menu"
```

## Quick Start

### Basic Dropdown

```vue
<template>
  <UiMenu>
    <UiMenuTrigger>
      <button>Options</button>
    </UiMenuTrigger>

    <UiMenuContent>
      <UiMenuItem @select="handleEdit">Edit</UiMenuItem>
      <UiMenuItem @select="handleDuplicate">Duplicate</UiMenuItem>
      <UiMenuSeparator />
      <UiMenuItem danger @select="handleDelete">Delete</UiMenuItem>
    </UiMenuContent>
  </UiMenu>
</template>

<script setup lang="ts">
function handleEdit() {
  console.log("Edit clicked")
}

function handleDuplicate() {
  console.log("Duplicate clicked")
}

function handleDelete() {
  console.log("Delete clicked")
}
</script>
```

### Context Menu (Right-Click)

```vue
<template>
  <div @contextmenu.prevent="openMenu">
    Right-click here
  </div>

  <UiMenu v-model:open="isOpen">
    <UiMenuContent :position="menuPosition">
      <UiMenuItem @select="handleCopy">Copy</UiMenuItem>
      <UiMenuItem @select="handlePaste">Paste</UiMenuItem>
    </UiMenuContent>
  </UiMenu>
</template>

<script setup lang="ts">
import { ref } from "vue"

const isOpen = ref(false)
const menuPosition = ref({ x: 0, y: 0 })

function openMenu(event: MouseEvent) {
  menuPosition.value = { x: event.clientX, y: event.clientY }
  isOpen.value = true
}

function handleCopy() {
  console.log("Copy")
}

function handlePaste() {
  console.log("Paste")
}
</script>
```

## Keyboard Interaction

### Trigger

| Key | Action |
|-----|--------|
| **Enter / Space** | Toggle menu |
| **ArrowDown** | Open and focus first item |

### Menu Content

| Key | Action |
|-----|--------|
| **ArrowUp / ArrowDown** | Navigate items |
| **Home / End** | Jump to first/last item |
| **Enter / Space** | Select item |
| **Escape** | Close and return focus to trigger |
| **Tab** | Close menu |

## Positioning

The menu system supports two positioning modes:

### Trigger-Based Positioning
Opens below the trigger element and automatically flips above if there's insufficient space below.

### Cursor-Based Positioning
Positions the menu at specific screen coordinates, useful for context menus.

The menu automatically repositions when:
- Window resizes
- Page scrolls
- Menu content changes size
- Trigger element moves or resizes

Positioning uses **ResizeObserver** for efficient DOM monitoring.

## Submenus

Nested menus with intelligent hover detection and keyboard navigation.

### Features
- Hover intent detection with mouse trajectory prediction
- Smooth keyboard navigation between levels
- Automatic positioning with viewport awareness
- Configurable hover delays
- Unlimited nesting depth

```vue
<template>
  <UiMenu>
    <UiMenuTrigger>
      <button>Actions</button>
    </UiMenuTrigger>

    <UiMenuContent>
      <UiMenuItem @select="handleOpen">Open File</UiMenuItem>

      <UiSubMenu>
        <UiSubMenuTrigger>File Actions</UiSubMenuTrigger>
        <UiSubMenuContent>
          <UiMenuItem @select="handleRename">Rename</UiMenuItem>
          <UiMenuItem @select="handleDuplicate">Duplicate</UiMenuItem>

          <UiSubMenu>
            <UiSubMenuTrigger>Advanced</UiSubMenuTrigger>
            <UiSubMenuContent>
              <UiMenuItem @select="handleCompress">Compress</UiMenuItem>
              <UiMenuItem @select="handleArchive">Archive</UiMenuItem>
            </UiSubMenuContent>
          </UiSubMenu>
        </UiSubMenuContent>
      </UiSubMenu>

      <UiMenuSeparator />

      <UiMenuItem danger @select="handleDelete">Delete</UiMenuItem>
    </UiMenuContent>
  </UiMenu>
</template>

<script setup lang="ts">
function handleOpen() { /* ... */ }
function handleRename() { /* ... */ }
function handleDuplicate() { /* ... */ }
function handleCompress() { /* ... */ }
function handleArchive() { /* ... */ }
function handleDelete() { /* ... */ }
</script>
```

## Theming

The menu system is fully customizable using CSS variables. All styling is theme-agnostic and works with any CSS framework.

### Default Theme

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

### Dark Mode

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

Apply dark mode globally:

```html
<html class="dark">
```

Or to specific sections:

```html
<div class="dark">
  <UiMenu />
</div>
```

### Custom Theme

Create custom themes by overriding CSS variables:

```css
.custom-theme {
  --ui-menu-bg: #f8f9fa;
  --ui-menu-text: #212529;
  --ui-menu-hover-bg: #e9ecef;
  --ui-menu-border: #dee2e6;
  --ui-menu-radius: 4px;
}
```

```html
<div class="custom-theme">
  <UiMenu />
</div>
```

## API Reference

### `<UiMenu>`
Root component that manages menu state and provides context to children.

**Props:**
- `open?: boolean` - Controls menu visibility (v-model compatible)

**Events:**
- `update:open` - Emitted when menu visibility changes

### `<UiMenuTrigger>`
Trigger element that opens the menu.

**Props:**
- `asChild?: boolean` - Pass trigger behavior to child element

### `<UiMenuContent>`
Floating container for menu items, teleported to document body.

**Props:**
- `position?: { x: number; y: number }` - Explicit positioning for context menus

### `<UiMenuItem>`
Selectable menu item.

**Props:**
- `danger?: boolean` - Applies danger/destructive styling

**Events:**
- `@select` - Emitted when item is selected

### `<UiMenuLabel>`
Non-interactive label for menu sections.

### `<UiMenuSeparator>`
Visual separator between menu items.

### `<UiSubMenu>`
Container for nested menu functionality.

### `<UiSubMenuTrigger>`
Trigger that opens a submenu on hover or keyboard interaction.

### `<UiSubMenuContent>`
Floating container for submenu items, automatically positioned.

## Accessibility

- Full keyboard navigation with arrow keys
- Roving focus management
- Proper ARIA roles (`menu`, `menuitem`, `separator`)
- Escape and Tab key handling
- Focus returns to trigger on close
- Screen reader compatible

## License

MIT
