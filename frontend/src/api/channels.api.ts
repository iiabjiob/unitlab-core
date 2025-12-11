import { http } from "./http"
import { API_V1, buildQuery } from "./utils"
import type { ChannelDto } from "@/types/channel"

export const ChannelsAPI = {
	list(params?: Record<string, any>) {
		return http.get<ChannelDto[]>(buildQuery(`${API_V1}/channels`, params))
	},

	get(id: number | string) {
		return http.get<ChannelDto>(`${API_V1}/channels/${id}`)
	},

	update(id: number | string, payload: Partial<ChannelDto>) {
		return http.patch<ChannelDto>(`${API_V1}/channels/${id}`, payload)
	},

	delete(id: number | string) {
		return http.delete(`${API_V1}/channels/${id}`)
	},
}

