import { http } from "./http"
import { API_V1 } from "./utils"
import type {
	Switchgear,
	SwitchgearCreateInput,
	SwitchgearUpdateInput,
} from "@/types/switchgear"

const basePath = (projectId: number | string) => `${API_V1}/projects/${projectId}/switchgears`

export const SwitchgearsAPI = {
	list(projectId: number | string) {
		return http.get<Switchgear[]>(basePath(projectId))
	},

	get(projectId: number | string, id: number | string) {
		return http.get<Switchgear>(`${basePath(projectId)}/${id}`)
	},

	create(projectId: number | string, payload: SwitchgearCreateInput) {
		return http.post<Switchgear>(basePath(projectId), payload)
	},

	update(projectId: number | string, id: number | string, payload: SwitchgearUpdateInput) {
		return http.patch<Switchgear>(`${basePath(projectId)}/${id}`, payload)
	},

	delete(projectId: number | string, id: number | string) {
		return http.delete(`${basePath(projectId)}/${id}`)
	},
}

