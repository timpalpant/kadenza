import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Flow {
    id: root
    visible: model.length > 0
    spacing: Kirigami.Units.smallSpacing

    property alias model: repeater.model
    property string selectedId: ""
    signal genreSelected(string id)
    readonly property var flow: root

    QQC2.Button {
        text: i18n("All Genres")
        checkable: true
        checked: root.selectedId.length === 0
        onClicked: {
            root.genreSelected("")
        }
    }

    Repeater {
        id: repeater
        delegate: QQC2.Button {
            text: modelData.name
            checkable: true
            checked: root.selectedId === modelData.id
            onClicked: {
                root.genreSelected(modelData.id)
            }
        }
    }
}
