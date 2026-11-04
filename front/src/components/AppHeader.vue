<script setup lang="ts">
defineProps<{ connected: boolean }>()
</script>
<template>
  <header class="topbar">
    <div class="brand"><span class="brand-mark">✦</span><div><strong>PIXELFORGE</strong><small>ESP32 LED CONTROL</small></div></div>
    <div class="connection"><i :class="{ online: connected }"></i>{{ connected ? 'Backend connecté' : 'Connexion...' }} <span>•</span> v1.0</div>
  </header>
</template>
<style scoped>
.topbar { height: 82px; display:flex; align-items:center; justify-content:space-between; border-bottom:1px solid #30352d }
.brand { display:flex; gap:12px; align-items:center; letter-spacing:.12em }.brand-mark { color:#c9ef5b; font-size:30px }.brand strong { display:block; font-size:14px }.brand small,.connection { font:10px 'DM Mono',monospace; letter-spacing:.1em; color:#899184 }.brand small { font-size:9px;color:#738065 }.connection { display:flex;gap:9px;align-items:center }.connection i { width:7px;height:7px;border-radius:50%;background:#696d63 }.connection i.online { background:#c9ef5b;box-shadow:0 0 12px #c9ef5b }
@media (max-width:560px) { .connection { display:none } }
</style>
