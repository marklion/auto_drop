<template>
<div class="app_show" id="app">
    <state></state>
</div>
</template>

<script>
import State from './components/State.vue'
export default {
    name: 'App',
    components:{
        "state":State
    },
    data: function () {
        return {

        }
    },
    methods: {},
    mounted: function () {}
}
</script>

<style>
</style>
