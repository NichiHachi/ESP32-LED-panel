<script setup lang="ts">
import { ref } from 'vue'
import type { HistoryItem } from '@/services/history'
defineProps<{ file: File | null; originalUrl: string; history: HistoryItem[]; dragging: boolean }>()
const emit = defineEmits<{ choose: [file: File | undefined]; drop: [event: DragEvent]; selectHistory: [item: HistoryItem]; removeHistory: [id: string] }>()
const activeTab = ref<'new' | 'history'>('new')
</script>
<template>
  <section class="card">
    <div class="card-heading"><div><p class="eyebrow">01 / SOURCE</p><h2>Votre visuel</h2></div><span class="badge">PNG · JPG · GIF · WEBP</span></div>
    <div class="tabs"><button class="tab" :class="{ active: activeTab === 'new' }" @click="activeTab = 'new'">Nouvelle image</button><button class="tab" :class="{ active: activeTab === 'history' }" @click="activeTab = 'history'" :disabled="!history.length">Historique ({{ history.length }})</button></div>
    <div v-if="activeTab === 'new'" class="dropzone" :class="{ dragging }" @dragover.prevent @dragenter.prevent @drop.prevent="$emit('drop', $event)">
      <input id="file" type="file" accept="image/*" @change="$emit('choose', ($event.target as HTMLInputElement).files?.[0])">
      <label for="file"><span class="upload-icon">↑</span><strong>{{ file ? file.name : 'Déposez votre image ici' }}</strong><small>{{ file ? `${(file.size / 1024 / 1024).toFixed(2)} MB · Prête à traiter` : 'ou cliquez pour parcourir vos fichiers' }}</small></label>
    </div>
    <div v-if="activeTab === 'new'" class="source-preview"><img v-if="originalUrl" :src="originalUrl" alt="Aperçu original"><span v-else>AUCUN APERÇU</span></div>
    <div v-else class="history-list"><div class="history-title">IMAGES RÉCENTES</div><div v-for="item in history" :key="item.id" class="history-item"><button @click="$emit('selectHistory', item)"><span>▣</span><span>{{ item.name }}</span><small>{{ new Date(item.createdAt).toLocaleDateString() }}</small></button><button class="delete" @click="$emit('removeHistory', item.id)">×</button></div></div>
  </section>
</template>
<style scoped>
.card { border:1px solid #353d31;background:rgba(25,29,23,.86);padding:25px;border-radius:3px }.card-heading{display:flex;justify-content:space-between;margin-bottom:14px}.eyebrow,.badge,.history-title{font:10px 'DM Mono',monospace;letter-spacing:.1em;color:#9ab366}.eyebrow{margin:0 0 12px}h2{font-size:22px;margin:0}.badge{border:1px solid #414a3b;padding:7px;color:#8c997a}.tabs{display:flex;gap:15px;margin-bottom:15px;border-bottom:1px solid #30382d}.tab{background:none;color:#77836e;padding:10px 0;border-radius:0;font-size:11px}.tab.active{color:#c9ef5b;border-bottom:2px solid #c9ef5b}.dropzone{height:128px;border:1px dashed #596b46;background:#1c2318}.dropzone.dragging,.dropzone:hover{border-color:#c9ef5b;background:#252e1d}.dropzone input{display:none}.dropzone label{height:100%;display:flex;flex-direction:column;justify-content:center;align-items:center;gap:8px;cursor:pointer}.upload-icon{font-size:26px;color:#c9ef5b}.dropzone strong{font-size:14px;font-weight:500}.dropzone small,.source-preview{color:#7e8877;font-size:12px}.source-preview{height:205px;margin-top:14px;display:flex;justify-content:center;align-items:center;background:#11140f;background-image:linear-gradient(45deg,#1a1e18 25%,transparent 25%),linear-gradient(-45deg,#1a1e18 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#1a1e18 75%),linear-gradient(-45deg,transparent 75%,#1a1e18 75%);background-size:20px 20px;overflow:hidden;font:10px 'DM Mono',monospace;letter-spacing:.15em}.source-preview img{max-width:100%;max-height:100%;object-fit:contain;image-rendering:pixelated}.history-list{margin-top:20px}.history-title{color:#7e8877;margin-bottom:8px}.history-item{display:flex;border-bottom:1px solid #30382d}.history-item>button:first-child{display:flex;align-items:center;gap:9px;flex:1;background:none;color:#bdc9b0;text-align:left;padding:9px 0;font-size:12px}.history-item small{margin-left:auto;color:#687363;font:10px 'DM Mono',monospace}.history-item span:first-child{color:#c9ef5b}.history-item .delete{background:none;color:#887d72;padding:7px}.tab:disabled{opacity:.5}
</style>
