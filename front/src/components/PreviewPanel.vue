<script setup lang="ts">
import AnimatedPreview from './AnimatedPreview.vue'
import type { ProcessedResult } from '@/types'
defineProps<{ resultUrl: string; result: ProcessedResult | null; loading: boolean; hasFile: boolean }>()
defineEmits<{ process: []; matrix: [] }>()
</script>
<template>
  <section class="card"><div class="card-heading"><div><p class="eyebrow">02 / APERÇU</p><h2>Rendu LED</h2></div><span class="status">{{ result ? '● RENDU PRÊT' : '○ EN ATTENTE' }}</span></div>
    <div class="result-preview"><img v-if="result?.blob && resultUrl" :src="resultUrl" alt="Rendu LED"><AnimatedPreview v-else-if="result?.frames" :frames="result.frames" /><span v-else><b>✦</b>Le rendu apparaîtra ici<br><small>après traitement de votre image</small></span></div>
    <div class="preview-actions"><button class="primary" :disabled="!hasFile || loading" @click="$emit('process')">{{ loading ? 'Traitement...' : 'Traiter l’image' }} <span>→</span></button><button class="secondary" :disabled="!hasFile || loading" @click="$emit('matrix')">Voir la matrice</button></div>
  </section>
</template>
<style scoped>
.card{border:1px solid #353d31;background:rgba(25,29,23,.86);padding:25px;border-radius:3px}.card-heading{display:flex;justify-content:space-between;margin-bottom:22px}.eyebrow{font:10px 'DM Mono',monospace;letter-spacing:.1em;color:#9ab366;margin:0 0 12px}h2{font-size:22px;margin:0}.status{font:10px 'DM Mono',monospace;color:#c9ef5b}.result-preview{height:300px;border:1px solid #2d3529;display:flex;justify-content:center;align-items:center;background:#11140f;background-image:linear-gradient(45deg,#1a1e18 25%,transparent 25%),linear-gradient(-45deg,#1a1e18 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#1a1e18 75%),linear-gradient(-45deg,transparent 75%,#1a1e18 75%);background-size:20px 20px;overflow:hidden}.result-preview img{width:100%;height:100%;object-fit:contain;image-rendering:pixelated}.result-preview span{text-align:center;line-height:1.8;color:#697363}.result-preview b{display:block;color:#c9ef5b;font-size:27px}.preview-actions{display:flex;gap:10px;margin-top:16px}button{border:0;border-radius:2px;padding:12px 16px;font-size:12px;font-weight:600}.primary{flex:1;background:#c9ef5b;color:#182013;text-align:left}.primary span{float:right;font-size:17px}.secondary{background:#293025;color:#bdc9b0}button:disabled{opacity:.4}
</style>
