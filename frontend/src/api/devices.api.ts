import { http } from "./http"
import { API_V1, buildQuery } from "./utils"
import type { DeviceDto, DeviceBulkDeleteResponse } from "@/types/device"
import type { ChannelDto, ChannelListDto } from "@/types/channel"

export const DevicesAPI = {
	list(params?: Record<string, any>) {
		return http.get<DeviceDto[]>(buildQuery(`${API_V1}/devices`, params))
	},

	get(id: number | string) {
		return http.get<DeviceDto>(`${API_V1}/devices/${id}`)
	},

	update(id: number | string, payload: Partial<DeviceDto>) {
		return http.patch<DeviceDto>(`${API_V1}/devices/${id}`, payload)
	},

	delete(id: number | string) {
		return http.delete(`${API_V1}/devices/${id}`)
	},

	bulkDelete(ids: Array<number | string>) {
		return http.delete<DeviceBulkDeleteResponse>(`${API_V1}/devices/bulk`, {
			data: { ids },
		})
	},

	getChannels(id: number | string, params?: Record<string, any>) {
		return http.get<ChannelListDto | ChannelDto[]>(buildQuery(`${API_V1}/devices/${id}/channels`, params))
	},
}
