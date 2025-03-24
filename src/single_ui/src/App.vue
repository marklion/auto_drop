<template>
<div class="app_show">
    <el-button type="primary" @click="get_all_json">获取</el-button>
    <div>{{current_state}}</div>
</div>
</template>

<script>
export default {
    name: 'App',
    data: function () {
        return {
            current_state: '',
        }
    },
    methods: {
        get_all_json: async function () {
            await this.$send_req('/redis/pub', {
                pub_path: 'current_state_',
                pub_req: ''
            });
            let current_state_resp = await this.$send_req('/redis/get', {
                key: 'current_state_'
            });
            this.current_state = current_state_resp.value;
        },
    },
    mounted: function () {}
}
</script>

<style>
</style>
