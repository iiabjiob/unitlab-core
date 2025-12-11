import { http } from "./http"
import { API_V1 } from "./utils"
import type {
	Switchgear,
	SwitchgearCreateInput,
	SwitchgearUpdateInput,
} from "@/types/switchgear"

export const SwitchgearsAPI = {
	list() {
		return http.get<Switchgear[]>(`${API_V1}/switchgears`)
	},

	get(id: number | string) {
		return http.get<Switchgear>(`${API_V1}/switchgears/${id}`)
	},

	create(payload: SwitchgearCreateInput) {
		return http.post<Switchgear>(`${API_V1}/switchgears`, payload)
	},

	update(id: number | string, payload: SwitchgearUpdateInput) {
		return http.patch<Switchgear>(`${API_V1}/switchgears/${id}`, payload)
	},

	delete(id: number | string) {
		return http.delete(`${API_V1}/switchgears/${id}`)
	},
}

