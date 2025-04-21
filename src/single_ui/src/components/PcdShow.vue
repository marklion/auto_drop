<template>
<div class="pcd_show">
    <canvas id="canvas"></canvas>
</div>
</template>

<script>
import * as THREE from 'three';
export default {
    name: 'PcdShow',
    data: function () {
        return {
            pcd: null,
            renderer: null,
            geometry: null,
            camera: null,
            scene: null,
        }
    },
    watch: {
        pcd: function () {
            const buffer = new Float32Array(this.pcd);
            const positions = new Float32Array(buffer.length / 7 * 3);
            const colors = new Float32Array(buffer.length / 7 * 3);

            // 解析二进制数据（格式：x,y,z,r,g,b）
            for (let i = 0; i < buffer.length; i += 7) {
                positions[i / 7 * 3] = buffer[i];
                positions[i / 7 * 3 + 1] = buffer[i + 1];
                positions[i / 7 * 3 + 2] = buffer[i + 2];

                colors[i / 7 * 3] = buffer[i + 3];
                colors[i / 7 * 3 + 1] = buffer[i + 4];
                colors[i / 7 * 3 + 2] = buffer[i + 5];
            }

            this.geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
            this.geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
            this.geometry.attributes.position.needsUpdate = true;
            this.animate();
        },
    },
    methods: {
        animate: function () {
            requestAnimationFrame(this.animate);
            this.renderer.render(this.scene, this.camera);
        },
        init_three: async function () {
            // 初始化场景
            const scene = new THREE.Scene();
            const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
            const renderer = new THREE.WebGLRenderer({
                antialias: true,
                canvas: document.getElementById('canvas')
            });
            this.renderer = renderer;
            camera.position.x = 200
            camera.position.y = 200
            camera.position.z = 200

            // 创建点云容器
            const geometry = new THREE.BufferGeometry();
            const material = new THREE.PointsMaterial({
                size: 0.1,
                vertexColors: true
            });
            const points = new THREE.Points(geometry, material);
            scene.add(points);
            this.geometry = geometry;
            this.scene = scene;
            this.camera = camera;
        },
        init_pcd_process: function () {
            const {
                DataSyncClient
            } = require('../../../public/pub_node_lib/websocket-data-sync.js')
            const client = new DataSyncClient('/ws/');
            client.watchData((key, value) => {
                if (key === 'pcd') {
                    // 解码 Base64 字符串
                    const decodedValue = atob(value.data);
                    // 将解码后的字符串转换为 Uint8Array
                    const uint8Array = new Uint8Array(decodedValue.length);
                    for (let i = 0; i < decodedValue.length; i++) {
                        uint8Array[i] = decodedValue.charCodeAt(i);
                    }
                    // 将解码后的数据赋值给 pcd
                    this.pcd = uint8Array;
                }
            }, 'pcd');
        }
    },
    mounted: async function () {
        await this.init_three();
        this.init_pcd_process();
    }
}
</script>

<style scoped>
.pcd_show {
    width: 100%;
    height: 60vh;
    /* 让容器占满整个视口高度 */
    display: flex;
    justify-content: center;
    align-items: center;
}

canvas {
    width: 90%;
    /* 设置 canvas 的宽度为容器的 90% */
    height: 90%;
    /* 设置 canvas 的高度为容器的 90% */
}
</style>
