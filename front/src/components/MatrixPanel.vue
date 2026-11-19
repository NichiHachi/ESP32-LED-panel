<script setup lang="ts">
import type { Matrix } from '@/types'
defineProps<{ matrix: Matrix }>()
</script>
<template><section class="card"><div class="heading"><div><p>04 / DONNÉES</p><h2>Matrice {{ matrix.width }} × {{ matrix.height }}</h2></div><span>JSON PIXELS</span></div><div class="pixel-grid" :style="{ gridTemplateColumns: `repeat(${matrix.width}, 1fr)` }"><i v-for="(pixel, index) in matrix.pixels" :key="index" :style="{ backgroundColor: `rgb(${pixel.r},${pixel.g},${pixel.b})` }"></i></div></section></template>
<style scoped>.card{border:1px solid #353d31;background:rgba(25,29,23,.86);padding:25px;margin-top:18px}.heading{display:flex;justify-content:space-between}.heading p,.heading span{font:10px 'DM Mono',monospace;color:#9ab366;letter-spacing:.1em}h2{font-size:22px;margin:0}.pixel-grid{display:grid;gap:2px;background:#050605;padding:3px;max-height:460px;overflow:auto;margin-top:22px}.pixel-grid i{aspect-ratio:1;min-width:2px}</style>
