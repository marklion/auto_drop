<template>
<div class="control_show">
    <div>
        <el-upload :file-list="always_empty" :show-file-list="false" action="/api/upload" :limit="1" :on-success="update_version">
            <el-button size="mini" type="primary">更新</el-button>
        </el-upload>
    </div>
    <div>
        <el-button size="mini" type="success" @click="control_gate(true)">开门</el-button>
        <el-button size="mini" type="danger" @click="control_gate(false)">关门</el-button>
    </div>

</div>
</template>

<script>
export default {
    name: 'Control-Page',
    data: function () {
        return {
            always_empty: [],
        }
    },
    methods: {
        control_gate: async function (is_open) {
            await this.$send_req('/redis/pub', {
                pub_req: {
                    dev_name: 'gate',
                    is_open: is_open,
                },
                pub_path: 'gate_ctrl_'
            });
        },
        update_version: async function (res) {
            await this.$send_req('/update', {
                file_path: res,
            });
            self.always_empty = [];
            window.location.reload();
        },
    },
}
</script>

<style scoped>
.control_show {
    display: flex;
    flex-direction: column;
    /* 保持子元素垂直排列 */
    justify-content: center;
    /* 垂直居中 */
    align-items: center;
    /* 水平居中 */
    height: 100vh;
    /* 让父容器占满视口高度 */
}
</style>
