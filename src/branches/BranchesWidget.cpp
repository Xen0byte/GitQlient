﻿#include "BranchesWidget.h"

#include <BranchTreeWidget.h>
#include <GitBase.h>
#include <GitSubmodules.h>
#include <GitStashes.h>
#include <GitSubtree.h>
#include <GitTags.h>
#include <GitConfig.h>
#include <BranchesViewDelegate.h>
#include <ClickableFrame.h>
#include <AddSubtreeDlg.h>
#include <StashesContextMenu.h>
#include <SubmodulesContextMenu.h>
#include <GitCache.h>
#include <GitQlientBranchItemRole.h>
#include <GitQlientSettings.h>
#include <BranchesWidgetMinimal.h>

#include <QApplication>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QLabel>
#include <QMenu>
#include <QHeaderView>
#include <QPushButton>
#include <QToolButton>
#include <QMessageBox>

#include <QLogger.h>

using namespace QLogger;
using namespace GitQlient;

namespace
{
QTreeWidgetItem *getChild(QTreeWidgetItem *parent, const QString &childName)
{
   QTreeWidgetItem *child = nullptr;

   if (parent)
   {
      const auto childrenCount = parent->childCount();

      for (auto i = 0; i < childrenCount; ++i)
         if (parent->child(i)->text(0) == childName)
            child = parent->child(i);
   }

   return child;
}
}

BranchesWidget::BranchesWidget(const QSharedPointer<GitCache> &cache, const QSharedPointer<GitBase> &git,
                               QWidget *parent)
   : QFrame(parent)
   , mCache(cache)
   , mGit(git)
   , mGitTags(new GitTags(mGit))
   , mLocalBranchesTree(new BranchTreeWidget(mGit))
   , mRemoteBranchesTree(new BranchTreeWidget(mGit))
   , mTagsTree(new QTreeWidget())
   , mStashesList(new QListWidget())
   , mStashesCount(new QLabel(tr("(0)")))
   , mStashesArrow(new QLabel())
   , mSubmodulesCount(new QLabel("(0)"))
   , mSubmodulesArrow(new QLabel())
   , mSubmodulesList(new QListWidget())
   , mSubtreeCount(new QLabel("(0)"))
   , mSubtreeArrow(new QLabel())
   , mSubtreeList(new QListWidget())
   , mMinimize(new QPushButton())
   , mMinimal(new BranchesWidgetMinimal(mCache, mGit))
{
   connect(mGitTags.data(), &GitTags::remoteTagsReceived, mCache.data(), &GitCache::updateTags);
   connect(mCache.get(), &GitCache::signalCacheUpdated, this, &BranchesWidget::processTags);

   setAttribute(Qt::WA_DeleteOnClose);

   mLocalBranchesTree->setLocalRepo(true);
   mLocalBranchesTree->setMouseTracking(true);
   mLocalBranchesTree->setItemDelegate(mLocalDelegate = new BranchesViewDelegate());
   mLocalBranchesTree->setColumnCount(1);
   mLocalBranchesTree->setObjectName("LocalBranches");

   const auto localHeader = mLocalBranchesTree->headerItem();
   localHeader->setText(0, tr("Local"));

   mRemoteBranchesTree->setColumnCount(1);
   mRemoteBranchesTree->setMouseTracking(true);
   mRemoteBranchesTree->setItemDelegate(mRemotesDelegate = new BranchesViewDelegate());

   const auto remoteHeader = mRemoteBranchesTree->headerItem();
   remoteHeader->setText(0, tr("Remote"));

   const auto tagHeader = mTagsTree->headerItem();
   tagHeader->setText(0, tr("Tags"));

   mTagsTree->setColumnCount(1);
   mTagsTree->setMouseTracking(true);
   mTagsTree->setItemDelegate(mTagsDelegate = new BranchesViewDelegate(true));
   mTagsTree->setContextMenuPolicy(Qt::CustomContextMenu);

   GitQlientSettings settings;

   /* STASHES START */
   if (const auto visible = settings.localValue(mGit->getGitQlientSettingsDir(), "StashesHeader", true).toBool();
       !visible)
   {
      const auto icon = QIcon(!visible ? QString(":/icons/add") : QString(":/icons/remove"));
      mStashesArrow->setPixmap(icon.pixmap(QSize(15, 15)));
      mStashesList->setVisible(visible);
   }
   else
      mStashesArrow->setPixmap(QIcon(":/icons/remove").pixmap(QSize(15, 15)));

   const auto stashHeaderFrame = new ClickableFrame();
   const auto stashHeaderLayout = new QHBoxLayout(stashHeaderFrame);
   stashHeaderLayout->setContentsMargins(10, 0, 0, 0);
   stashHeaderLayout->setSpacing(10);
   stashHeaderLayout->addWidget(new QLabel(tr("Stashes")));
   stashHeaderLayout->addWidget(mStashesCount);
   stashHeaderLayout->addStretch();
   stashHeaderLayout->addWidget(mStashesArrow);

   mStashesList->setMouseTracking(true);
   mStashesList->setContextMenuPolicy(Qt::CustomContextMenu);

   const auto stashLayout = new QVBoxLayout();
   stashLayout->setContentsMargins(QMargins());
   stashLayout->setSpacing(0);
   stashLayout->addWidget(stashHeaderFrame);
   stashLayout->addSpacing(5);
   stashLayout->addWidget(mStashesList);

   const auto stashFrame = new QFrame();
   stashFrame->setObjectName("sectionFrame");
   stashFrame->setLayout(stashLayout);

   /* STASHES END */

   /* SUBMODULES START */
   if (const auto visible = settings.localValue(mGit->getGitQlientSettingsDir(), "SubmodulesHeader", true).toBool();
       !visible)
   {
      const auto icon = QIcon(!visible ? QString(":/icons/add") : QString(":/icons/remove"));
      mSubmodulesArrow->setPixmap(icon.pixmap(QSize(15, 15)));
      mSubmodulesList->setVisible(visible);
   }
   else
      mSubmodulesArrow->setPixmap(QIcon(":/icons/remove").pixmap(QSize(15, 15)));

   const auto submoduleHeaderFrame = new ClickableFrame();
   const auto submoduleHeaderLayout = new QHBoxLayout(submoduleHeaderFrame);
   submoduleHeaderLayout->setContentsMargins(10, 0, 0, 0);
   submoduleHeaderLayout->setSpacing(10);
   submoduleHeaderLayout->addWidget(new QLabel(tr("Submodules")));
   submoduleHeaderLayout->addWidget(mSubmodulesCount);
   submoduleHeaderLayout->addStretch();
   submoduleHeaderLayout->addWidget(mSubmodulesArrow);

   mSubmodulesList->setMouseTracking(true);
   mSubmodulesList->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(mSubmodulesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
      emit signalOpenSubmodule(mGit->getWorkingDir().append("/").append(item->text()));
   });

   const auto submoduleLayout = new QVBoxLayout();
   submoduleLayout->setContentsMargins(QMargins());
   submoduleLayout->setSpacing(0);
   submoduleLayout->addWidget(submoduleHeaderFrame);
   submoduleLayout->addSpacing(5);
   submoduleLayout->addWidget(mSubmodulesList);

   const auto submoduleFrame = new QFrame();
   submoduleFrame->setObjectName("sectionFrame");
   submoduleFrame->setLayout(submoduleLayout);

   /* SUBMODULES END */

   /* SUBTREE START */
   if (const auto visible = settings.localValue(mGit->getGitQlientSettingsDir(), "SubtreeHeader", true).toBool();
       !visible)
   {
      const auto icon = QIcon(!visible ? QString(":/icons/add") : QString(":/icons/remove"));
      mSubtreeArrow->setPixmap(icon.pixmap(QSize(15, 15)));
      mSubtreeList->setVisible(visible);
   }
   else
      mSubtreeArrow->setPixmap(QIcon(":/icons/remove").pixmap(QSize(15, 15)));

   const auto subtreeHeaderFrame = new ClickableFrame();
   const auto subtreeHeaderLayout = new QHBoxLayout(subtreeHeaderFrame);
   subtreeHeaderLayout->setContentsMargins(10, 0, 0, 0);
   subtreeHeaderLayout->setSpacing(10);
   subtreeHeaderLayout->addWidget(new QLabel(tr("Subtrees")));
   subtreeHeaderLayout->addWidget(mSubtreeCount = new QLabel(tr("(0)")));
   subtreeHeaderLayout->addStretch();
   subtreeHeaderLayout->addWidget(mSubtreeArrow);

   mSubtreeList->setMouseTracking(true);
   mSubtreeList->setContextMenuPolicy(Qt::CustomContextMenu);

   const auto subtreeLayout = new QVBoxLayout();
   subtreeLayout->setContentsMargins(QMargins());
   subtreeLayout->setSpacing(0);
   subtreeLayout->addWidget(subtreeHeaderFrame);
   subtreeLayout->addSpacing(5);
   subtreeLayout->addWidget(mSubtreeList);

   const auto subtreeFrame = new QFrame();
   subtreeFrame->setObjectName("sectionFrame");
   subtreeFrame->setLayout(subtreeLayout);

   /* SUBTREE END */

   const auto searchBranch = new QLineEdit();
   searchBranch->setPlaceholderText(tr("Prese ENTER to search a branch..."));
   searchBranch->setObjectName("SearchInput");
   connect(searchBranch, &QLineEdit::returnPressed, this, &BranchesWidget::onSearchBranch);

   mMinimize->setIcon(QIcon(":/icons/ahead"));
   mMinimize->setToolTip(tr("Show minimalist view"));
   mMinimize->setObjectName("BranchesWidgetOptionsButton");
   connect(mMinimize, &QPushButton::clicked, this, &BranchesWidget::minimalView);

   const auto mainControlsLayout = new QHBoxLayout();
   mainControlsLayout->setContentsMargins(QMargins());
   mainControlsLayout->setSpacing(5);
   mainControlsLayout->addWidget(mMinimize);
   mainControlsLayout->addWidget(searchBranch);

   const auto panelsLayout = new QVBoxLayout();
   panelsLayout->setContentsMargins(QMargins());
   panelsLayout->setSpacing(0);
   panelsLayout->addWidget(mLocalBranchesTree);
   panelsLayout->addWidget(mRemoteBranchesTree);
   panelsLayout->addWidget(mTagsTree);
   panelsLayout->addWidget(stashFrame);
   panelsLayout->addWidget(submoduleFrame);
   panelsLayout->addWidget(subtreeFrame);

   const auto panelsFrame = new QFrame();
   panelsFrame->setObjectName("panelsFrame");
   panelsFrame->setLayout(panelsLayout);

   const auto vLayout = new QVBoxLayout();
   vLayout->setContentsMargins(0, 0, 10, 0);
   vLayout->setSpacing(0);
   vLayout->addLayout(mainControlsLayout);
   vLayout->addSpacing(5);
   vLayout->addWidget(panelsFrame);

   mFullBranchFrame = new QFrame();
   mFullBranchFrame->setObjectName("FullBranchesWidget");
   const auto mainBranchLayout = new QHBoxLayout(mFullBranchFrame);
   mainBranchLayout->setContentsMargins(QMargins());
   mainBranchLayout->setSpacing(0);
   mainBranchLayout->addLayout(vLayout);

   const auto mainLayout = new QGridLayout(this);
   mainLayout->setContentsMargins(QMargins());
   mainLayout->setSpacing(0);
   mainLayout->addWidget(mFullBranchFrame, 0, 0, 3, 1);
   mainLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding), 0, 1);
   mainLayout->addWidget(mMinimal, 1, 1);
   mainLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding), 2, 1);

   const auto isMinimalVisible
       = settings.localValue(mGit->getGitQlientSettingsDir(), "MinimalBranchesView", false).toBool();
   mFullBranchFrame->setVisible(!isMinimalVisible);
   mMinimal->setVisible(isMinimalVisible);
   connect(mMinimal, &BranchesWidgetMinimal::showFullBranchesView, this, &BranchesWidget::fullView);
   connect(mMinimal, &BranchesWidgetMinimal::commitSelected, this, &BranchesWidget::signalSelectCommit);
   connect(mMinimal, &BranchesWidgetMinimal::stashSelected, this, &BranchesWidget::onStashSelected);

   /*
   connect(mLocalBranchesTree, &BranchTreeWidget::signalRefreshPRsCache, mCache.get(),
           &GitCache::refreshPRsCache);
*/
   connect(mLocalBranchesTree, &BranchTreeWidget::signalSelectCommit, this, &BranchesWidget::signalSelectCommit);
   connect(mLocalBranchesTree, &BranchTreeWidget::signalSelectCommit, mRemoteBranchesTree,
           &BranchTreeWidget::clearSelection);
   connect(mLocalBranchesTree, &BranchTreeWidget::signalFetchPerformed, mGitTags.data(), &GitTags::getRemoteTags);
   connect(mLocalBranchesTree, &BranchTreeWidget::signalBranchesUpdated, this, &BranchesWidget::signalBranchesUpdated);
   connect(mLocalBranchesTree, &BranchTreeWidget::signalBranchCheckedOut, this,
           &BranchesWidget::signalBranchCheckedOut);
   connect(mLocalBranchesTree, &BranchTreeWidget::signalMergeRequired, this, &BranchesWidget::signalMergeRequired);
   connect(mLocalBranchesTree, &BranchTreeWidget::signalPullConflict, this, &BranchesWidget::signalPullConflict);
   connect(mRemoteBranchesTree, &BranchTreeWidget::signalSelectCommit, this, &BranchesWidget::signalSelectCommit);
   connect(mRemoteBranchesTree, &BranchTreeWidget::signalSelectCommit, mLocalBranchesTree,
           &BranchTreeWidget::clearSelection);
   connect(mRemoteBranchesTree, &BranchTreeWidget::signalFetchPerformed, mGitTags.data(), &GitTags::getRemoteTags);
   connect(mRemoteBranchesTree, &BranchTreeWidget::signalBranchesUpdated, this, &BranchesWidget::signalBranchesUpdated);
   connect(mRemoteBranchesTree, &BranchTreeWidget::signalBranchCheckedOut, this,
           &BranchesWidget::signalBranchCheckedOut);
   connect(mRemoteBranchesTree, &BranchTreeWidget::signalMergeRequired, this, &BranchesWidget::signalMergeRequired);

   connect(mTagsTree, &QTreeWidget::itemClicked, this, &BranchesWidget::onTagClicked);
   connect(mTagsTree, &QListWidget::customContextMenuRequested, this, &BranchesWidget::showTagsContextMenu);
   connect(mStashesList, &QListWidget::itemClicked, this, &BranchesWidget::onStashClicked);
   connect(mStashesList, &QListWidget::customContextMenuRequested, this, &BranchesWidget::showStashesContextMenu);
   connect(mSubmodulesList, &QListWidget::customContextMenuRequested, this, &BranchesWidget::showSubmodulesContextMenu);
   connect(mSubtreeList, &QListWidget::customContextMenuRequested, this, &BranchesWidget::showSubtreesContextMenu);
   connect(stashHeaderFrame, &ClickableFrame::clicked, this, &BranchesWidget::onStashesHeaderClicked);
   connect(submoduleHeaderFrame, &ClickableFrame::clicked, this, &BranchesWidget::onSubmodulesHeaderClicked);
   connect(subtreeHeaderFrame, &ClickableFrame::clicked, this, &BranchesWidget::onSubtreesHeaderClicked);
}

BranchesWidget::~BranchesWidget()
{
   delete mLocalDelegate;
   delete mRemotesDelegate;
   delete mTagsDelegate;
}

void BranchesWidget::showBranches()
{
   QLog_Info("UI", QString("Loading branches data"));

   clear();
   mMinimal->clearActions();

   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

   auto branches = mCache->getBranches(References::Type::LocalBranch);

   if (!branches.empty())
   {
      QLog_Info("UI", QString("Fetched {%1} local branches").arg(branches.count()));
      QLog_Info("UI", QString("Processing local branches..."));

      for (const auto &pair : branches)
      {
         for (const auto &branch : pair.second)
         {
            if (!branch.contains("HEAD->"))
            {
               processLocalBranch(pair.first, branch);
               mMinimal->configureLocalMenu(pair.first, branch);
            }
         }
      }

      QLog_Info("UI", QString("... local branches processed"));
   }

   branches = mCache->getBranches(References::Type::RemoteBranches);

   if (!branches.empty())
   {
      QLog_Info("UI", QString("Fetched {%1} remote branches").arg(branches.count()));
      QLog_Info("UI", QString("Processing remote branches..."));

      for (const auto &pair : qAsConst(branches))
      {
         for (const auto &branch : pair.second)
         {
            if (!branch.contains("HEAD->"))
            {
               processRemoteBranch(pair.first, branch);
               mMinimal->configureRemoteMenu(pair.first, branch);
            }
         }
      }

      QLog_Info("UI", QString("... remote branches processed"));
   }

   processTags();
   processStashes();
   processSubmodules();
   processSubtrees();

   QApplication::restoreOverrideCursor();

   adjustBranchesTree(mLocalBranchesTree);
}

void BranchesWidget::refreshCurrentBranchLink()
{
   mLocalBranchesTree->reloadCurrentBranchLink();
}

void BranchesWidget::clear()
{
   blockSignals(true);
   mLocalBranchesTree->clear();
   mRemoteBranchesTree->clear();
   mTagsTree->clear();
   blockSignals(false);
}

void BranchesWidget::fullView()
{
   mFullBranchFrame->setVisible(true);
   mMinimal->setVisible(false);

   GitQlientSettings settings;
   settings.setLocalValue(mGit->getGitQlientSettingsDir(), "MinimalBranchesView", mMinimal->isVisible());
}

void BranchesWidget::returnToSavedView()
{
   GitQlientSettings settings;
   const auto savedState = settings.localValue(mGit->getGitQlientSettingsDir(), "MinimalBranchesView", false).toBool();

   if (savedState != mMinimal->isVisible())
   {
      mFullBranchFrame->setVisible(!savedState);
      mMinimal->setVisible(savedState);
   }
}

void BranchesWidget::minimalView()
{
   forceMinimalView();

   GitQlientSettings settings;
   settings.setLocalValue(mGit->getGitQlientSettingsDir(), "MinimalBranchesView", mMinimal->isVisible());
}

void BranchesWidget::forceMinimalView()
{
   mFullBranchFrame->setVisible(false);
   mMinimal->setVisible(true);
}

void BranchesWidget::onPanelsVisibilityChaned()
{
   GitQlientSettings settings;

   auto visible = settings.localValue(mGit->getGitQlientSettingsDir(), "StashesHeader", true).toBool();
   auto icon = QIcon(!visible ? QString(":/icons/add") : QString(":/icons/remove"));
   mStashesArrow->setPixmap(icon.pixmap(QSize(15, 15)));
   mStashesList->setVisible(visible);

   visible = settings.localValue(mGit->getGitQlientSettingsDir(), "SubmodulesHeader", true).toBool();
   icon = QIcon(!visible ? QString(":/icons/add") : QString(":/icons/remove"));
   mSubmodulesArrow->setPixmap(icon.pixmap(QSize(15, 15)));
   mSubmodulesList->setVisible(visible);

   visible = settings.localValue(mGit->getGitQlientSettingsDir(), "SubtreeHeader", true).toBool();
   icon = QIcon(!visible ? QString(":/icons/add") : QString(":/icons/remove"));
   mSubtreeArrow->setPixmap(icon.pixmap(QSize(15, 15)));
   mSubtreeList->setVisible(visible);
}

void BranchesWidget::processLocalBranch(const QString &sha, QString branch)
{
   QLog_Debug("UI", QString("Adding local branch {%1}").arg(branch));

   auto isCurrentBranch = false;

   if (branch == mGit->getCurrentBranch())
      isCurrentBranch = true;

   const auto fullBranchName = branch;

   QVector<QTreeWidgetItem *> parents;
   QTreeWidgetItem *parent = nullptr;
   auto folders = branch.split("/");
   branch = folders.takeLast();

   for (const auto &folder : qAsConst(folders))
   {
      QTreeWidgetItem *child = nullptr;

      if (parent)
      {
         child = getChild(parent, folder);
         parents.append(child);
      }
      else
      {
         for (auto i = 0; i < mLocalBranchesTree->topLevelItemCount(); ++i)
         {
            if (mLocalBranchesTree->topLevelItem(i)->text(0) == folder)
            {
               child = mLocalBranchesTree->topLevelItem(i);
               parents.append(child);
            }
         }
      }

      if (!child)
      {
         const auto item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem();
         item->setText(0, folder);

         if (!parent)
            mLocalBranchesTree->addTopLevelItem(item);

         parent = item;
         parents.append(parent);
      }
      else
      {
         parent = child;
         parents.append(child);
      }
   }

   auto item = new QTreeWidgetItem(parent);
   item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
   item->setText(0, branch);
   item->setData(0, GitQlient::IsCurrentBranchRole, isCurrentBranch);
   item->setData(0, GitQlient::FullNameRole, fullBranchName);
   item->setData(0, GitQlient::LocalBranchRole, true);
   item->setData(0, GitQlient::ShaRole, sha);
   item->setData(0, Qt::ToolTipRole, fullBranchName);
   item->setData(0, GitQlient::IsLeaf, true);

   if (isCurrentBranch)
   {
      item->setSelected(true);

      for (const auto parent : parents)
      {
         mLocalBranchesTree->setCurrentItem(item);
         mLocalBranchesTree->expandItem(parent);
         const auto indexToScroll = mLocalBranchesTree->currentIndex();
         mLocalBranchesTree->scrollTo(indexToScroll);
      }
   }

   mLocalBranchesTree->addTopLevelItem(item);

   QLog_Debug("UI", QString("Finish gathering local branch information"));
}

void BranchesWidget::processRemoteBranch(const QString &sha, QString branch)
{
   const auto fullBranchName = branch;
   auto folders = branch.split("/");
   branch = folders.takeLast();

   QTreeWidgetItem *parent = nullptr;

   for (const auto &folder : qAsConst(folders))
   {
      QTreeWidgetItem *child = nullptr;

      if (parent)
         child = getChild(parent, folder);
      else
      {
         for (auto i = 0; i < mRemoteBranchesTree->topLevelItemCount(); ++i)
         {
            if (mRemoteBranchesTree->topLevelItem(i)->text(0) == folder)
               child = mRemoteBranchesTree->topLevelItem(i);
         }
      }

      if (!child)
      {
         const auto item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem();
         item->setText(0, folder);

         if (!parent)
         {
            item->setData(0, GitQlient::IsRoot, true);
            mRemoteBranchesTree->addTopLevelItem(item);
         }

         parent = item;
      }
      else
         parent = child;
   }

   QLog_Debug("UI", QString("Adding remote branch {%1}").arg(branch));

   const auto item = new QTreeWidgetItem(parent);
   item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
   item->setText(0, branch);
   item->setData(0, GitQlient::FullNameRole, fullBranchName);
   item->setData(0, GitQlient::LocalBranchRole, false);
   item->setData(0, GitQlient::ShaRole, sha);
   item->setData(0, Qt::ToolTipRole, fullBranchName);
   item->setData(0, GitQlient::IsLeaf, true);
}

void BranchesWidget::processTags()
{
   const auto localTags = mCache->getTags(References::Type::LocalTag);
   const auto remoteTags = mCache->getTags(References::Type::RemoteTag);

   for (const auto &localTag : localTags.toStdMap())
   {
      QTreeWidgetItem *parent = nullptr;
      auto tagName = localTag.first;
      auto folders = tagName.split("/");

      for (const auto &folder : qAsConst(folders))
      {
         QTreeWidgetItem *child = nullptr;

         if (parent)
            child = getChild(parent, folder);
         else
         {
            for (auto i = 0; i < mTagsTree->topLevelItemCount(); ++i)
            {
               if (mTagsTree->topLevelItem(i)->text(0) == folder)
                  child = mTagsTree->topLevelItem(i);
            }
         }

         if (!child)
         {
            const auto item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem();
            item->setText(0, folder);

            if (!parent)
               mTagsTree->addTopLevelItem(item);

            parent = item;
         }
         else
            parent = child;
      }

      const auto item = new QTreeWidgetItem(parent);
      item->setData(0, Qt::UserRole, tagName);
      item->setData(0, Qt::UserRole + 2, localTag.second);

      if (!remoteTags.contains(tagName))
      {
         tagName += " (local)";
         item->setData(0, Qt::UserRole + 1, false);
      }
      else
         item->setData(0, Qt::UserRole + 1, true);

      item->setText(0, tagName);

      mTagsTree->addTopLevelItem(item);

      mMinimal->configureTagsMenu(localTag.second, tagName);
   }

   // mTagsList->clear();
}

void BranchesWidget::processStashes()
{
   mStashesList->clear();

   QScopedPointer<GitStashes> git(new GitStashes(mGit));
   const auto stashes = git->getStashes();

   QLog_Info("UI", QString("Fetching {%1} stashes").arg(stashes.count()));

   for (const auto &stash : stashes)
   {
      const auto stashId = stash.split(":").first();
      const auto stashDesc = stash.split("}: ").last();
      const auto item = new QListWidgetItem(stashDesc);
      item->setData(Qt::UserRole, stashId);
      mStashesList->addItem(item);
      mMinimal->configureStashesMenu(stashId, stashDesc);
   }

   mStashesCount->setText(QString("(%1)").arg(stashes.count()));
}

void BranchesWidget::processSubmodules()
{
   mSubmodulesList->clear();

   QScopedPointer<GitSubmodules> git(new GitSubmodules(mGit));
   const auto submodules = git->getSubmodules();

   QLog_Info("UI", QString("Fetching {%1} submodules").arg(submodules.count()));

   for (const auto &submodule : submodules)
   {
      mSubmodulesList->addItem(submodule);
      mMinimal->configureSubmodulesMenu(submodule);
   }

   mSubmodulesCount->setText('(' + QString::number(submodules.count()) + ')');
}

void BranchesWidget::processSubtrees()
{
   mSubtreeList->clear();

   QScopedPointer<GitSubtree> git(new GitSubtree(mGit));

   const auto ret = git->list();

   if (ret.success)
   {
      const auto rawData = ret.output.toString();
      const auto commits = rawData.split("\n\n");
      auto count = 0;

      for (auto &subtreeRawData : commits)
      {
         if (!subtreeRawData.isEmpty())
         {
            QString name;
            QString sha;
            auto fields = subtreeRawData.split("\n");

            for (auto &field : fields)
            {
               if (field.contains("git-subtree-dir:"))
                  name = field.remove("git-subtree-dir:").trimmed();
               else if (field.contains("git-subtree-split"))
                  sha = field.remove("git-subtree-split:").trimmed();
            }

            mSubtreeList->addItem(name);
            ++count;
         }
      }

      mSubtreeCount->setText('(' + QString::number(count) + ')');
   }
}

void BranchesWidget::adjustBranchesTree(BranchTreeWidget *treeWidget)
{
   for (auto i = 1; i < treeWidget->columnCount(); ++i)
      treeWidget->resizeColumnToContents(i);

   treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);

   for (auto i = 1; i < treeWidget->columnCount(); ++i)
      treeWidget->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

   treeWidget->header()->setStretchLastSection(false);
}

void BranchesWidget::showTagsContextMenu(const QPoint &p)
{
   QModelIndex index = mTagsTree->indexAt(p);

   if (!index.isValid())
      return;

   const auto tagName = index.data(Qt::UserRole).toString();
   const auto isRemote = index.data(Qt::UserRole + 1).toBool();
   const auto menu = new QMenu(this);
   const auto removeTagAction = menu->addAction(tr("Remove tag"));
   connect(removeTagAction, &QAction::triggered, this, [this, tagName, isRemote]() {
      QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
      QScopedPointer<GitTags> git(new GitTags(mGit));
      const auto ret = git->removeTag(tagName, isRemote);
      QApplication::restoreOverrideCursor();

      if (ret.success)
         emit signalBranchesUpdated();
   });

   const auto pushTagAction = menu->addAction(tr("Push tag"));
   pushTagAction->setEnabled(!isRemote);
   connect(pushTagAction, &QAction::triggered, this, [this, tagName]() {
      QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
      QScopedPointer<GitTags> git(new GitTags(mGit));
      const auto ret = git->pushTag(tagName);
      QApplication::restoreOverrideCursor();

      if (ret.success)
         emit signalBranchesUpdated();
   });

   menu->exec(mTagsTree->viewport()->mapToGlobal(p));
}

void BranchesWidget::showStashesContextMenu(const QPoint &p)
{
   QLog_Info("UI", QString("Requesting context menu for stashes"));

   const auto index = mStashesList->indexAt(p);

   if (index.isValid())
   {
      const auto menu = new StashesContextMenu(mGit, index.data(Qt::UserRole).toString(), this);
      connect(menu, &StashesContextMenu::signalUpdateView, this, &BranchesWidget::signalBranchesUpdated);
      connect(menu, &StashesContextMenu::signalContentRemoved, this, &BranchesWidget::signalBranchesUpdated);
      menu->exec(mStashesList->viewport()->mapToGlobal(p));
   }
}

void BranchesWidget::showSubmodulesContextMenu(const QPoint &p)
{
   QLog_Info("UI", QString("Requesting context menu for submodules"));

   const auto menu = new SubmodulesContextMenu(mGit, mSubmodulesList->indexAt(p), this);
   connect(menu, &SubmodulesContextMenu::openSubmodule, this, &BranchesWidget::signalOpenSubmodule);
   connect(menu, &SubmodulesContextMenu::infoUpdated, this, &BranchesWidget::signalBranchesUpdated);

   menu->exec(mSubmodulesList->viewport()->mapToGlobal(p));
}

void BranchesWidget::showSubtreesContextMenu(const QPoint &p)
{
   QLog_Info("UI", QString("Requesting context menu for subtrees"));

   QModelIndex index = mSubtreeList->indexAt(p);

   const auto menu = new QMenu(this);

   if (index.isValid())
   {
      connect(menu->addAction(tr("Pull")), &QAction::triggered, this, [this, index]() {
         QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

         const auto prefix = index.data().toString();
         const auto subtreeData = getSubtreeData(prefix);

         QScopedPointer<GitSubtree> git(new GitSubtree(mGit));
         const auto ret = git->pull(subtreeData.first, subtreeData.second, prefix);
         QApplication::restoreOverrideCursor();

         if (ret.success)
            emit signalBranchesUpdated();
         else
            QMessageBox::warning(this, tr("Error when pulling"), ret.output.toString());
      });
      /*
      connect(menu->addAction(tr("Merge")), &QAction::triggered, this, [this, index]() {
         QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

         const auto subtreeData = getSubtreeData(index.data().toString());

         QScopedPointer<GitSubtree> git(new GitSubtree(mGit));
         const auto ret = git->pull(subtreeData.first, subtreeData.second);
         QApplication::restoreOverrideCursor();

         if (ret.success)
            emit signalBranchesUpdated();
      });
*/
      connect(menu->addAction(tr("Push")), &QAction::triggered, this, [this, index]() {
         QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

         const auto prefix = index.data().toString();
         const auto subtreeData = getSubtreeData(prefix);

         QScopedPointer<GitSubtree> git(new GitSubtree(mGit));
         const auto ret = git->push(subtreeData.first, subtreeData.second, prefix);
         QApplication::restoreOverrideCursor();

         if (ret.success)
            emit signalBranchesUpdated();
         else
            QMessageBox::warning(this, tr("Error when pushing"), ret.output.toString());
      });

      const auto addSubtree = menu->addAction(tr("Configure"));
      connect(addSubtree, &QAction::triggered, this, [this, index]() {
         const auto prefix = index.data().toString();
         const auto subtreeData = getSubtreeData(prefix);
         AddSubtreeDlg addDlg(prefix, subtreeData.first, subtreeData.second, mGit);
         const auto ret = addDlg.exec();
         if (ret == QDialog::Accepted)
            emit signalBranchesUpdated();
      });
      // menu->addAction(tr("Split"));
   }
   else
   {
      const auto addSubtree = menu->addAction(tr("Add subtree"));
      connect(addSubtree, &QAction::triggered, this, [this]() {
         AddSubtreeDlg addDlg(mGit);
         const auto ret = addDlg.exec();
         if (ret == QDialog::Accepted)
            emit signalBranchesUpdated();
      });
   }

   menu->exec(mSubtreeList->viewport()->mapToGlobal(p));
}

void BranchesWidget::onStashesHeaderClicked()
{
   const auto stashesAreVisible = mStashesList->isVisible();
   const auto icon = QIcon(stashesAreVisible ? QString(":/icons/add") : QString(":/icons/remove"));
   mStashesArrow->setPixmap(icon.pixmap(QSize(15, 15)));
   mStashesList->setVisible(!stashesAreVisible);

   GitQlientSettings settings;
   settings.setLocalValue(mGit->getGitQlientSettingsDir(), "StashesHeader", !stashesAreVisible);

   emit panelsVisibilityChanged();
}

void BranchesWidget::onSubmodulesHeaderClicked()
{
   const auto submodulesAreVisible = mSubmodulesList->isVisible();
   const auto icon = QIcon(submodulesAreVisible ? QString(":/icons/add") : QString(":/icons/remove"));
   mSubmodulesArrow->setPixmap(icon.pixmap(QSize(15, 15)));
   mSubmodulesList->setVisible(!submodulesAreVisible);

   GitQlientSettings settings;
   settings.setLocalValue(mGit->getGitQlientSettingsDir(), "SubmodulesHeader", !submodulesAreVisible);

   emit panelsVisibilityChanged();
}

void BranchesWidget::onSubtreesHeaderClicked()
{
   const auto subtreesAreVisible = mSubtreeList->isVisible();
   const auto icon = QIcon(subtreesAreVisible ? QString(":/icons/add") : QString(":/icons/remove"));
   mSubtreeArrow->setPixmap(icon.pixmap(QSize(15, 15)));
   mSubtreeList->setVisible(!subtreesAreVisible);

   emit panelsVisibilityChanged();
}

void BranchesWidget::onTagClicked(QTreeWidgetItem *item)
{
   emit signalSelectCommit(item->data(0, Qt::UserRole + 2).toString());
}

void BranchesWidget::onStashClicked(QListWidgetItem *item)
{
   onStashSelected(item->data(Qt::UserRole).toString());
}

void BranchesWidget::onStashSelected(const QString &stashId)
{
   QScopedPointer<GitTags> git(new GitTags(mGit));
   const auto sha = git->getTagCommit(stashId).output.toString();

   emit signalSelectCommit(sha);
}

void BranchesWidget::onSearchBranch()
{
   const auto lineEdit = qobject_cast<QLineEdit *>(sender());

   const auto text = lineEdit->text();

   if (mLastSearch != text)
   {
      mLastSearch = text;
      mLastIndex = mLocalBranchesTree->focusOnBranch(text);
      mLastTreeSearched = mLocalBranchesTree;

      if (mLastIndex == -1)
      {
         mLastIndex = mRemoteBranchesTree->focusOnBranch(mLastSearch);
         mLastTreeSearched = mRemoteBranchesTree;

         if (mLastIndex == -1)
            mLastTreeSearched = mLocalBranchesTree;
      }
   }
   else
   {
      if (mLastTreeSearched == mLocalBranchesTree)
      {
         if (mLastIndex != -1)
         {
            mLastIndex = mLocalBranchesTree->focusOnBranch(mLastSearch, mLastIndex);
            mLastTreeSearched = mLocalBranchesTree;
         }

         if (mLastIndex == -1)
         {
            mLastIndex = mRemoteBranchesTree->focusOnBranch(mLastSearch);
            mLastTreeSearched = mRemoteBranchesTree;
         }
      }
      else if (mLastIndex != -1)
      {
         mLastIndex = mRemoteBranchesTree->focusOnBranch(mLastSearch, mLastIndex);
         mLastTreeSearched = mRemoteBranchesTree;

         if (mLastIndex == -1)
            mLastTreeSearched = mLocalBranchesTree;
      }
   }
}

QPair<QString, QString> BranchesWidget::getSubtreeData(const QString &prefix)
{
   GitQlientSettings settings;
   bool end = false;
   QString url;
   QString ref;

   for (auto i = 0; !end; ++i)
   {
      const auto repo = settings.localValue(mGit->getGitQlientSettingsDir(), QString("Subtrees/%1.prefix").arg(i), "");

      if (repo.toString() == prefix)
      {
         auto tmpUrl
             = settings.localValue(mGit->getGitQlientSettingsDir(), QString("Subtrees/%1.url").arg(i)).toString();
         auto tmpRef
             = settings.localValue(mGit->getGitQlientSettingsDir(), QString("Subtrees/%1.ref").arg(i)).toString();

         if (tmpUrl.isEmpty() || tmpRef.isEmpty())
         {
            const auto resp
                = QMessageBox::question(this, tr("Subtree configuration not found!"),
                                        tr("The subtree configuration was not found. It could be that it was created "
                                           "outside GitQlient.<br>To operate with this subtree, it needs to be "
                                           "configured.<br><br><b>Do you want to configure it now?<b>"));

            if (resp == QMessageBox::Yes)
            {
               AddSubtreeDlg stDlg(prefix, mGit, this);
               const auto ret = stDlg.exec();

               if (ret == QDialog::Accepted)
               {
                  tmpUrl = settings.localValue(mGit->getGitQlientSettingsDir(), QString("Subtrees/%1.url").arg(i))
                               .toString();
                  tmpRef = settings.localValue(mGit->getGitQlientSettingsDir(), QString("Subtrees/%1.ref").arg(i))
                               .toString();

                  if (tmpUrl.isEmpty() || tmpRef.isEmpty())
                     QMessageBox::critical(this, tr("Unexpected error!"),
                                           tr("An unidentified error happened while using subtrees. Please contact the "
                                              "creator of GitQlient for support."));
                  else
                  {
                     url = tmpUrl;
                     ref = tmpRef;
                  }
               }
            }

            end = true;
         }
         else
         {
            url = tmpUrl;
            ref = tmpRef;
            end = true;
         }
      }
      else if (repo.toString().isEmpty())
      {
         const auto resp
             = QMessageBox::question(this, tr("Subtree configuration not found!"),
                                     tr("The subtree configuration was not found. It could be that it was created "
                                        "outside GitQlient.<br>To operate with this subtree, it needs to be "
                                        "configured.<br><br><b>Do you want to configure it now?<b>"));

         if (resp == QMessageBox::Yes)
         {
            AddSubtreeDlg stDlg(prefix, mGit, this);
            const auto ret = stDlg.exec();

            if (ret == QDialog::Accepted)
            {
               const auto tmpUrl
                   = settings.localValue(mGit->getGitQlientSettingsDir(), QString("Subtrees/%1.url").arg(i)).toString();
               const auto tmpRef
                   = settings.localValue(mGit->getGitQlientSettingsDir(), QString("Subtrees/%1.ref").arg(i)).toString();

               if (tmpUrl.isEmpty() || tmpRef.isEmpty())
                  QMessageBox::critical(this, tr("Unexpected error!"),
                                        tr("An unidentified error happened while using subtrees. Please contact the "
                                           "creator of GitQlient for support."));
               else
               {
                  url = tmpUrl;
                  ref = tmpRef;
               }
            }
         }

         end = true;
      }
   }

   return qMakePair(url, ref);
}
