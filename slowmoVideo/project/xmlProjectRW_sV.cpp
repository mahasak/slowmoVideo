#include "xmlProjectRW_sV.h"

#include <QDebug>
#include <QTextStream>

#include "project_sV.h"
#include "nodelist_sV.h"
#include "../lib/defs_sV.h"

XmlProjectRW_sV::XmlProjectRW_sV()
{
}

int XmlProjectRW_sV::saveProject(const Project_sV *project, QString filename) const
{
    QDomDocument doc;
    QDomElement root = doc.createElement("sVproject");
    doc.appendChild(root);

    // File info
    QDomElement info = doc.createElement("info");
    root.appendChild(info);
    QDomElement appName = doc.createElement("appName");
    appName.appendChild(doc.createTextNode("slowmoVideo"));
    info.appendChild(appName);


    // Project Preferences
    QDomElement preferences = doc.createElement("preferences");
    root.appendChild(preferences);
    QDomElement renderFPS = doc.createElement("renderFPS");
    renderFPS.appendChild(doc.createTextNode(QString("%1").arg(project->fpsOut())));
    preferences.appendChild(renderFPS);
    QDomElement renderSize = doc.createElement("renderSize");
    renderSize.appendChild(doc.createTextNode(enumStr(project->renderFrameSize())));
    preferences.appendChild(renderSize);


    // Project Resources
    QDomElement resources = doc.createElement("resources");
    root.appendChild(resources);
    QDomElement inputFile = doc.createElement("inputFile");
    inputFile.appendChild(doc.createTextNode(project->inFileStr()));
    resources.appendChild(inputFile);


    // Nodes
    QDomElement nodes = doc.createElement("nodes");
    root.appendChild(nodes);
    NodeList_sV *nodeList = project->nodes();
    for (int i = 0; i < nodeList->size(); i++) {
        nodes.appendChild(nodeToDom(&doc, &nodeList->at(i)));
    }

    qDebug() << doc.toString(2);

    QFile outFile(filename);
    if (!outFile.open(QIODevice::WriteOnly)) {
        Q_ASSERT(false);
    }
    QTextStream output(&outFile);
    doc.save(output, 4);
    output.flush();

    return 0;
}

const QDomElement XmlProjectRW_sV::nodeToDom(QDomDocument *doc, const Node_sV *node)
{
    QDomElement el = doc->createElement("node");
    QDomElement x = doc->createElement("x");
    QDomElement y = doc->createElement("y");
    QDomElement selected = doc->createElement("selected");
    el.appendChild(x);
    el.appendChild(y);
    el.appendChild(selected);
    x.appendChild(doc->createTextNode(QString("%1").arg(node->xUnmoved())));
    y.appendChild(doc->createTextNode(QString("%1").arg(node->yUnmoved())));
    selected.appendChild(doc->createTextNode(QString("%1").arg(node->selected())));
    return el;
}

Project_sV* XmlProjectRW_sV::loadProject(QString filename) const
{
    return NULL;
}
