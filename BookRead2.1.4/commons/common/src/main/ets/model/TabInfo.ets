export interface TabInfo {
  index: number
  label: string
  activeIcon: Resource
  defaultIcon: Resource
  flag?: boolean //首页tab悬浮标识
  component?: WrappedBuilder<[]>
}