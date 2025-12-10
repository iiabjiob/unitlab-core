<template>
  <div class="flex h-full">

    <ResizablePanel
        placement="left"
        storageKey="page-sidebar-width"
        :defaultSize="240"
        :minSize="200"
        :maxSize="400"
        >
        <aside class="p-4 h-full flex flex-col">
            <DeviceListSidebar />

            <UiMenu>
            <!-- Top-level trigger -->
            <UiMenuTrigger asChild>
              <button class="px-3 py-2 bg-neutral-200 text-neutral-900 dark:bg-neutral-800 dark:text-neutral-200 rounded">
                Main menu
              </button>
            </UiMenuTrigger>

            <UiMenuContent>
              <UiMenuItem @select="handleNew">New File</UiMenuItem>
              <UiMenuItem @select="handleOpen">Open...</UiMenuItem>

              <UiMenuSeparator />

              <!-- Submenu -->
              <UiSubMenu>
                <UiSubMenuTrigger>
                  Export
                </UiSubMenuTrigger>

                <UiSubMenuContent>
                  <UiMenuItem @select="exportPDF">Export as PDF</UiMenuItem>
                  <UiMenuItem @select="exportCSV">Export as CSV</UiMenuItem>
                  <UiMenuItem @select="exportPNG">Export as PNG</UiMenuItem>
                </UiSubMenuContent>
              </UiSubMenu>

              <UiMenuSeparator />

              <UiMenuItem danger @select="handleDelete">
                Delete
              </UiMenuItem>
            </UiMenuContent>
          </UiMenu>


        </aside>
    </ResizablePanel>
    
    <section class="flex-1 p-4">
      <router-view />
    </section>
  </div>
</template>

<script setup lang="ts">
import ResizablePanel from '@/components/ui/ResizablePanel.vue';
import DeviceListSidebar from './components/DeviceListSidebar.vue';
import UiMenu from '@/components/ui/menu/UiMenu.vue';
import UiMenuTrigger from '@/components/ui/menu/UiMenuTrigger.vue';
import UiMenuContent from '@/components/ui/menu/UiMenuContent.vue';
import UiMenuItem from '@/components/ui/menu/UiMenuItem.vue';
import UiMenuSeparator from '@/components/ui/menu/UiMenuSeparator.vue';
import UiSubMenu from '@/components/ui/menu/UiSubMenu.vue';
import UiSubMenuTrigger from '@/components/ui/menu/UiSubMenuTrigger.vue';
import UiSubMenuContent from '@/components/ui/menu/UiSubMenuContent.vue';

function handleNew() { console.log("New File") }
function handleOpen() { console.log("Open…") }
function handleDelete() { console.log("Delete") }

function exportPDF() { console.log("Export PDF") }
function exportCSV() { console.log("Export CSV") }
function exportPNG() { console.log("Export PNG") }
</script>
