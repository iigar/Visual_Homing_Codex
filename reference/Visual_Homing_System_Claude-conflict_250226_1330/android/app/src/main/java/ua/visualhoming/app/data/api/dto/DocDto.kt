package ua.visualhoming.app.data.api.dto

data class DocFileDto(
    val name: String = "",
    val path: String = "",
    val title: String = ""
)

data class DocContentDto(
    val name: String = "",
    val title: String = "",
    val content: String = "",
    val html: String? = null
)
