import Vue from 'vue'
import ElementUI from 'element-ui'
import 'element-ui/lib/theme-chalk/index.css'
import App from './App.vue'
import send_req from '@/lib/request'

Vue.config.productionTip = false

Vue.use(ElementUI)

Vue.prototype.$send_req = send_req.send_req;

new Vue({
  el:'#app',
  render: h => h(App),
}).$mount('#app')
