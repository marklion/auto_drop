const { defineConfig } = require('@vue/cli-service')
const webpack = require('webpack')

const port = process.env.port || process.env.npm_config_port || 9528 // dev port
module.exports = defineConfig({
  publicPath: process.env.NODE_ENV === 'production' ? './' : '/',
  transpileDependencies: true,
  devServer: {
    port: port,
    open: true,
    hot: true,
    proxy: {
      '/api': {
        target: 'http://localhost:38010',
        changeOrigin: true,
      },
    }
  },
  configureWebpack: {
    resolve: {
      fallback: {
        "util": require.resolve("util/"),
        "stream": require.resolve("stream-browserify"),
        "crypto": require.resolve("crypto-browserify"),
        "dns": false,
        "net": false,
        "tls": false,
        "url": require.resolve("url/"),
        "assert": require.resolve("assert/")
      }
    },
    plugins: [
      new webpack.ProvidePlugin({
        process: 'process/browser',
        Buffer: ['buffer', 'Buffer']
      })
    ],

  }
})
