#include "GroupManager.hpp"
#include "Group.hpp"

GroupManager::GroupManager(QObject* parent):
    IdentifiedObject<GroupManager>{id_type<GroupManager>{0}, "GroupManager", parent}
{

}

void GroupManager::addGroup(Group* group)
{
    qDebug( ) << "Adding group to " << (void* ) this;
    m_groups.push_back(group);
    emit groupAdded(group->id());
}

void GroupManager::removeGroup(id_type<Group> group)
{
    using namespace std;

    auto it = find(begin(m_groups), end(m_groups), group);
    m_groups.erase(it);

    emit groupRemoved(group);

    (*it)->deleteLater();
}

Group* GroupManager::group(const id_type<Group>& id) const
{
    return *std::find(std::begin(m_groups), std::end(m_groups), id);
}

id_type<Group> GroupManager::defaultGroup() const
{

    qDebug( ) << "Asking group at " << (void* ) this;
    return m_groups[0]->id();
}

const std::vector<Group*>& GroupManager::groups() const
{
    return m_groups;
}
