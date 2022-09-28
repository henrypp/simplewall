----
name: Question
about: Ask a question about application.
title: '[Question]'
labels: [ "question" ]
assignees: ''

----
body:
  - type: markdown
    attributes:
      value: Thanks for taking the time to fill out this question!
  - type: checkboxes
    id: checklist
    attributes:
      label: Checklist
      options:
        - label: I have used the search function to see if someone else has already submitted the same question.
          required: true
        - label: I will describe the question with as much detail as possible.
          required: true
  - type: input
    id: version
    attributes:
      label: App version
      description: What the application version do you use.
      placeholder: x.y.x
    validations:
      required: true
  - type: input
    id: windows_version
    attributes:
      label: Windows version
      description: What Windows version do you use.
  - type: textarea
    id: question
    attributes:
      label: Question
      placeholder: |
        Describe question as much detail as possible
    validations:
      required: true
  - type: textarea
    id: additional
    attributes:
      label: Additional information
      description: Provide additional information that can help.
