export interface Project {
  id: number
  uuid: string
  name: string
  created_at: string
  updated_at: string
}

export interface ProjectCreateInput {
  name: string
}

export interface ProjectUpdateInput {
  name?: string
}
