const path = require('path')

module.exports = {
    target: "node",
    entry: './src/index.js',
    output: {
        path: path.resolve(__dirname, 'build/api'),
        filename: 'index.js'
    },
    resolve: {
        modules: ['node_modules', path.resolve(__dirname, 'node_modules')],
    },
    mode: 'development',
    devtool: 'source-map',
}