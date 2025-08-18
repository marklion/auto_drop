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
            function voxelGridFilter(buffer, voxelSize) {
                const voxelMap = new Map();

                for (let i = 0; i < buffer.length; i += 7) {
                    const x = Math.floor(buffer[i] / voxelSize);
                    const y = Math.floor(buffer[i + 1] / voxelSize);
                    const z = Math.floor(buffer[i + 2] / voxelSize);

                    const key = `${x},${y},${z}`;
                    if (!voxelMap.has(key)) {
                        voxelMap.set(key, [
                            buffer[i],     // x
                            buffer[i + 1], // y
                            buffer[i + 2], // z
                            buffer[i + 3], // r
                            buffer[i + 4], // g
                            buffer[i + 5]  // b
                        ]);
                    }
                }

                const sampledBuffer = Array.from(voxelMap.values()).flat();
                return new Float32Array(sampledBuffer);
            }

            const voxelSize = 0.05; // 设置体素大小
            const filteredBuffer = voxelGridFilter(new Float32Array(this.pcd.buffer), voxelSize);

            // 解析过滤后的点云数据
            const positions = new Float32Array(filteredBuffer.length / 6 * 3);
            const colors = new Float32Array(filteredBuffer.length / 6 * 3);

            for (let i = 0; i < filteredBuffer.length; i += 6) {
                positions[i / 6 * 3] = filteredBuffer[i];
                positions[i / 6 * 3 + 1] = filteredBuffer[i + 1];
                positions[i / 6 * 3 + 2] = filteredBuffer[i + 2];

                colors[i / 6 * 3] = filteredBuffer[i + 3];
                colors[i / 6 * 3 + 1] = filteredBuffer[i + 4];
                colors[i / 6 * 3 + 2] = filteredBuffer[i + 5];
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
            const camera = new THREE.PerspectiveCamera(90, window.innerWidth / window.innerHeight, 1, 10);
            const renderer = new THREE.WebGLRenderer({
                antialias: true,
                canvas: document.getElementById('canvas')
            });
            this.renderer = renderer;
            camera.position.x = 0
            camera.position.y = -2
            camera.position.z = 0
            camera.up.set(0, 0, 1);
            camera.lookAt(0, 0, -2.3);

            // 创建点云容器
            const geometry = new THREE.BufferGeometry();
            const material = new THREE.PointsMaterial({
                size: 0.00000000001,
                vertexColors: true
            });
            const points = new THREE.Points(geometry, material);
            scene.add(points);

            const axesHelper = new THREE.AxesHelper(5); // 创建一个坐标轴辅助工具，长度为 5
            scene.add(axesHelper); // 将坐标轴添加到场景中

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
