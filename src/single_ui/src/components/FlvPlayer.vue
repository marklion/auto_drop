<template>
<div class="flv-container">
    <el-input placeholder="请输入内容" v-model="url">
        <el-button slot="append" icon="el-icon-search" @click="initFlvPlayer"></el-button>
    </el-input>
    <video ref="videoElement" controls width="100%" height="500"></video>
</div>
</template>

<script>
import flvjs from 'flv.js';

export default {
    name: 'FlvPlayer',
    data() {
        return {
            flvPlayer: null,
            url: '',
        };
    },
    mounted() {
        this.initFlvPlayer();
    },
    beforeDestroy() {
        this.destroyPlayer();
    },
    methods: {
        // 初始化播放器 [3,5](@ref)
        initFlvPlayer() {
            if (!this.url) {
                console.error('URL不能为空');
                return;
            }
            if (flvjs.isSupported()) {
                const videoElement = this.$refs.videoElement;
                this.flvPlayer = flvjs.createPlayer({
                    type: 'flv',
                    isLive: true,
                    hasAudio: false,
                    url: this.url,
                });
                this.flvPlayer.attachMediaElement(videoElement);
                this.flvPlayer.load();
                // 在加载完成后立即播放
                this.flvPlayer.play().catch((error) => {
                    console.error('播放失败:', error);
                });
            } else {
                console.error('当前浏览器不支持flv.js');
            }
        },

        // 销毁播放器 [1,3](@ref)
        destroyPlayer() {
            if (this.flvPlayer) {
                this.flvPlayer.pause();
                this.flvPlayer.unload();
                this.flvPlayer.detachMediaElement();
                this.flvPlayer.destroy();
                this.flvPlayer = null;
            }
        }
    }
};
</script>

<style scoped>
.flv-container {
    position: relative;
    margin: 20px;
}

button {
    margin-top: 10px;
    padding: 8px 16px;
}
</style>
