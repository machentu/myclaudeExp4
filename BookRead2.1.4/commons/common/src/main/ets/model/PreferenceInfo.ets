export class PreferenceInfo {
  genderOption: GenderOption = GenderOption.UNSELECTED
  novelType: string[] = []
}

export enum GenderOption {
  UNSELECTED,
  MALE,
  FEMALE,
  BOTH,
}
