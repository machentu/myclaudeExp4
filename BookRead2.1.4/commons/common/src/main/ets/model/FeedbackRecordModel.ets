export interface FeedbackItemType {
  description: string
  stageImageUris: Array<string>
  phone: string

}

@ObservedV2
export class FeedbackItem {
  @Trace description: string
  @Trace stageImageUris: Array<string>
  @Trace phone: string

  constructor( description: string, stageImageUris: Array<string>, phone: string) {
    this.description = description
    this.stageImageUris = stageImageUris
    this.phone = phone
  }
}

export class FeedbackRecord {
  feedbackList: FeedbackItem[] = []
}
