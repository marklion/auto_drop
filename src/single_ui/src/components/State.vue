<template>
<div>
    <el-card>
        <el-descriptions title="设备状态" :column="3" size="medium">
            <el-descriptions-item label="当前车号">{{current_state.plate}}</el-descriptions-item>
            <el-descriptions-item label="道闸状态">
                <el-tag size="mini" type="success" v-if="!current_state.gate">开启</el-tag>
                <el-tag type="danger" size="mini" v-else>关闭</el-tag>
            </el-descriptions-item>
            <el-descriptions-item label="溜槽开度">
                <div style="width: 200px">
                    <el-progress :percentage="current_state.lc_open_threshold"></el-progress>
                </div>
            </el-descriptions-item>
            <el-descriptions-item label="货满高度偏差">{{current_state.rd_full_offset}}</el-descriptions-item>
            <el-descriptions-item label="车辆位置">
                <el-tag type="primary" size="mini" v-if="current_state.rd_position == 0">初段</el-tag>
                <el-tag type="success" size="mini" v-else-if="current_state.rd_position == 1">中段</el-tag>
                <el-tag type="warning" size="mini" v-else-if="current_state.rd_position == 2">末段</el-tag>
                <el-tag type="danger" size="mini" v-else>无车辆</el-tag>
            </el-descriptions-item>
            <el-descriptions-item label="料堆状态">
                <el-tag type="success" size="mini" v-if="!current_state.rd_full">未满</el-tag>
                <el-tag type="danger" size="mini" v-else>满</el-tag>
            </el-descriptions-item>
            <el-descriptions-item label="地磅读数">{{current_state.scale_weight.toFixed(2)}}</el-descriptions-item>
        </el-descriptions>
    </el-card>
</div>
</template>

<script>
export default {
    name: 'App',
    data: function () {
        return {
            current_state: {
                plate: '未知',
                gate: false,
                lc_open_threshold: 0,
                rd_position: 0,
                rd_full: false,
                scale_weight: 0,
                rd_full_offset: 0,
            },
        }
    },
    methods: {
        init_ws_process: function () {
            const {
                DataSyncClient
            } = require('../../../public/pub_node_lib/websocket-data-sync.js')
            const client = new DataSyncClient('/ws/');
            client.watchData((key, value) => {
                if (key === 'current_state_') {
                    this.current_state = value;
                }
            }, 'current_state_');
        }
    },
    mounted: function () {
        this.init_ws_process();
    }
}
</script>

<style>
</style>
