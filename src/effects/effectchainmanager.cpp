#include "effects/effectchainmanager.h"

#include <QtDebug>
#include <QDomDocument>
#include <QFile>
#include <QDir>

#include "effects/effectsmanager.h"
#include "util/xml.h"

EffectChainManager::EffectChainManager(UserSettingsPointer pConfig,
                                       EffectsManager* pEffectsManager)
        : QObject(pEffectsManager),
          m_pConfig(pConfig),
          m_pEffectsManager(pEffectsManager) {
}

EffectChainManager::~EffectChainManager() {
    //qDebug() << debugString() << "destroyed";
}

void EffectChainManager::registerChannel(const ChannelHandleAndGroup& handle_group) {
    if (m_registeredChannels.contains(handle_group)) {
        qWarning() << debugString() << "WARNING: Channel already registered:"
                   << handle_group.name();
        return;
    }
    m_registeredChannels.insert(handle_group);

    foreach (StandardEffectRackPointer pRack, m_standardEffectRacks) {
        pRack->registerChannel(handle_group);
    }
}

StandardEffectRackPointer EffectChainManager::addStandardEffectRack() {
    StandardEffectRackPointer pRack(new StandardEffectRack(
        m_pEffectsManager, this, m_standardEffectRacks.size()));
    m_standardEffectRacks.append(pRack);
    m_effectRacksByGroup.insert(pRack->getGroup(), pRack);
    return pRack;
}

StandardEffectRackPointer EffectChainManager::getStandardEffectRack(int i) {
    if (i < 0 || i >= m_standardEffectRacks.size()) {
        return StandardEffectRackPointer();
    }
    return m_standardEffectRacks[i];
}

EqualizerRackPointer EffectChainManager::addEqualizerRack() {
    EqualizerRackPointer pRack(new EqualizerRack(
        m_pEffectsManager, this, m_equalizerEffectRacks.size()));
    m_equalizerEffectRacks.append(pRack);
    m_effectRacksByGroup.insert(pRack->getGroup(), pRack);
    return pRack;
}

EqualizerRackPointer EffectChainManager::getEqualizerRack(int i) {
    if (i < 0 || i >= m_equalizerEffectRacks.size()) {
        return EqualizerRackPointer();
    }
    return m_equalizerEffectRacks[i];
}

QuickEffectRackPointer EffectChainManager::addQuickEffectRack() {
    QuickEffectRackPointer pRack(new QuickEffectRack(
        m_pEffectsManager, this, m_quickEffectRacks.size()));
    m_quickEffectRacks.append(pRack);
    m_effectRacksByGroup.insert(pRack->getGroup(), pRack);
    return pRack;
}

QuickEffectRackPointer EffectChainManager::getQuickEffectRack(int i) {
    if (i < 0 || i >= m_quickEffectRacks.size()) {
        return QuickEffectRackPointer();
    }
    return m_quickEffectRacks[i];
}

EffectRackPointer EffectChainManager::getEffectRack(const QString& group) {
    return m_effectRacksByGroup.value(group);
}

void EffectChainManager::addEffectChain(EffectChainPointer pEffectChain) {
    if (pEffectChain) {
        m_effectChains.append(pEffectChain);
    }
}

void EffectChainManager::removeEffectChain(EffectChainPointer pEffectChain) {
    if (pEffectChain) {
        m_effectChains.removeAll(pEffectChain);
    }
}

EffectChainPointer EffectChainManager::getNextEffectChain(EffectChainPointer pEffectChain) {
    if (m_effectChains.isEmpty())
        return EffectChainPointer();

    if (!pEffectChain) {
        return m_effectChains[0];
    }

    int indexOf = m_effectChains.lastIndexOf(pEffectChain);
    if (indexOf == -1) {
        qWarning() << debugString() << "WARNING: getNextEffectChain called for an unmanaged EffectChain";
        return m_effectChains[0];
    }

    return m_effectChains[(indexOf + 1) % m_effectChains.size()];
}

EffectChainPointer EffectChainManager::getPrevEffectChain(EffectChainPointer pEffectChain) {
    if (m_effectChains.isEmpty())
        return EffectChainPointer();

    if (!pEffectChain) {
        return m_effectChains[m_effectChains.size()-1];
    }

    int indexOf = m_effectChains.lastIndexOf(pEffectChain);
    if (indexOf == -1) {
        qWarning() << debugString() << "WARNING: getPrevEffectChain called for an unmanaged EffectChain";
        return m_effectChains[m_effectChains.size()-1];
    }

    return m_effectChains[(indexOf - 1 + m_effectChains.size()) % m_effectChains.size()];
}

bool EffectChainManager::saveEffectChains() {
    QDomDocument doc("MixxxEffects");

    QString blank = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<MixxxEffects>\n"
        "</MixxxEffects>\n";
    doc.setContent(blank);

    QDomElement rootNode = doc.documentElement();
    foreach(EffectRackPointer pRack, m_standardEffectRacks) {
        rootNode.appendChild(pRack->toXML(&doc));
    }
    // TODO? Save QuickEffects in effects.xml too, or keep stored in ConfigObjects?
//     foreach(EffectRackPointer pRack, m_quickEffectRacks) {
//         rootNode.appendChild(pRack->toXML(&doc));
//     }

    QDir settingsPath(m_pConfig->getSettingsPath());

    if (!settingsPath.exists()) {
        return false;
    }

    QFile file(settingsPath.absoluteFilePath("effects.xml"));

    // TODO(rryan): overwrite the right way.
    if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        return false;
    }

    QString effectsXml = doc.toString();
    file.write(effectsXml.toUtf8());
    file.close();
    return true;
}

QList<EffectChainPointer> EffectChainManager::loadEffectChains() {
    QList<EffectChainPointer> loadedChains;

    QDir settingsPath(m_pConfig->getSettingsPath());
    QFile file(settingsPath.absoluteFilePath("effects.xml"));

    if (!file.open(QIODevice::ReadOnly)) {
        EffectChainPointer pEmptyChain;
        for (int i = 0; i < 4; ++i) {
            pEmptyChain = EffectChainPointer(new EffectChain(m_pEffectsManager,
                                                            QString(),
                                                            EffectChainPointer()));
            loadedChains.append(pEmptyChain);
        }
        return loadedChains;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        file.close();
        EffectChainPointer pEmptyChain;
        for (int i = 0; i < 4; ++i) {
            pEmptyChain = EffectChainPointer(new EffectChain(m_pEffectsManager,
                                                            QString(),
                                                            EffectChainPointer()));
            loadedChains.append(pEmptyChain);
        }
        return loadedChains;
    }
    file.close();

    QDomElement root = doc.documentElement();
    QDomElement rackElement = XmlParse::selectElement(root, "Rack");
    QDomElement chainsElement = XmlParse::selectElement(rackElement, "Chains");
    QDomNodeList chainsList = chainsElement.elementsByTagName("EffectChain");

    for (int i = 0; i < chainsList.count(); ++i) {
        QDomNode chainNode = chainsList.at(i);

        if (chainNode.isElement()) {
            EffectChainPointer pChain = EffectChain::fromXML(
                m_pEffectsManager, chainNode.toElement());

            loadedChains.append(pChain);
            m_effectChains.append(pChain);
        }
    }
    return loadedChains;
}
