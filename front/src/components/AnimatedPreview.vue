<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import type { AnimationFrame } from '@/types'
const props = defineProps<{ frames: AnimationFrame[] }>()
const current = ref(0)
const frame = computed(() => props.frames[current.value] || props.frames[0])
let timer: number | undefined
function play() { window.clearTimeout(timer); if (props.frames.length < 2 || !frame.value) return; timer = window.setTimeout(() => { current.value = (current.value + 1) % props.frames.length; play() }, frame.value.delayMs || 100) }
watch(() => props.frames, () => { current.value = 0; play() }, { immediate: true })
onBeforeUnmount(() => window.clearTimeout(timer))
</script>
<template><img v-if="frame" :src="`data:image/png;base64,${frame.pngBase64}`" alt="Rendu GIF animé"></template>
<style scoped>
img {
  width: 100%;
  height: 100%;
  object-fit: contain;
  image-rendering: pixelated;
}
</style>
