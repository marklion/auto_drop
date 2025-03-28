<template>
<div class="app_show" id="app">
    <state></state>
    <el-row :gutter="10" type="flex">
        <el-col :span="8">
            <flv-player></flv-player>
        </el-col>
        <el-col :span="8"></el-col>
        <el-col :span="8">
            <flv-player></flv-player>
        </el-col>
    </el-row>
</div>
</template>

<script>
import FlvPlayer from './components/FlvPlayer.vue'
import State from './components/State.vue'
export default {
    name: 'App',
    components: {
        "state": State,
        "flv-player": FlvPlayer
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
