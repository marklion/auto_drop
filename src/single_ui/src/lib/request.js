import axios from 'axios'
import { Message } from 'element-ui'
import { Loading } from 'element-ui'

// create an axios instance
function get_axios() {
    const service = axios.create({
        baseURL: process.env.VUE_APP_BASE_API, // url = base url + request url
        // withCredentials: true, // send cookies when cross-domain requests
        timeout: 10000 // request timeout
    })
    // request interceptor
    service.interceptors.request.use(
        config => {
            return config
        },
        error => {
            // do something with request error
            console.log(error) // for debug
            return Promise.reject(error)
        }
    )

    // response interceptor
    service.interceptors.response.use(
        response => {
            const res = response.data
            // if the custom code is not 20000, it is judged as an error.
            if (res.err_msg.length > 0) {
                Message({
                    message: res.err_msg || 'Error',
                    type: 'error',
                    duration: 10 * 1000
                })
                return Promise.reject(new Error(res.err_msg || 'Error'))
            } else {
                return res.result;
            }
        },
        error => {
            console.log('err' + error) // for debug
            Message({
                message: error.message,
                type: 'error',
                duration: 5 * 1000
            })
            return Promise.reject(error)
        }
    )
    return service;
}


export default {
    send_req: async function (url, data) {
        let li = Loading.service({
            fullscreen: true,
        });
        let resp;
        const service = get_axios();
        try {
            resp = await service({
                url: url,
                method: 'post',
                data
            })
            console.log(resp);

        } catch (error) {
            console.log(error);
            throw error;
        } finally {
            li.close();
        }
        return resp;

    }
}
