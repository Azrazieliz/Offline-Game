#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"

struct sqlite3;

class FOGSQLiteWorldStore final : public IOGWorldStore
{
public:
    FOGSQLiteWorldStore() = default;
    virtual ~FOGSQLiteWorldStore() override;

    virtual bool Open(const FString&, FString&) override;
    virtual void Close() override;
    virtual bool IsOpen() const override { return Database != nullptr; }

    virtual bool BeginTransaction(FString&) override;
    virtual bool CommitTransaction(FString&) override;
    virtual bool RollbackTransaction(FString&) override;
    virtual int32 GetSchemaVersion(FString&) const override;

    static int32 LatestSchemaVersion();
    bool RunApplicationValidation(FString& OutReport, FString& OutError) const;

    // Persistence-internal migration helpers. This concrete header lives under
    // Private; gameplay/UI code must continue to use IOGWorldStore instead.
    bool ExecuteSql(const FString&, FString&) const;

    virtual bool UpsertEntity(const FOGEntityId&, FName, int64, const FString&, FString&) override;
    virtual bool TryReadEntity(const FOGEntityId&, bool&, FName&, FString&, int64&, FString&) const override;

    virtual bool UpsertCharacterManifestation(const FOGCharacterManifestationRecord&, int64, FString&) override;
    virtual bool TryReadCharacterManifestation(const FOGEntityId&, bool&, FOGCharacterManifestationRecord&, FString&) const override;
    virtual bool TryFindCharacterManifestationByOwnerAndIdentity(const FOGEntityId&, const FOGContentId&, bool&, FOGCharacterManifestationRecord&, FString&) const override;

    virtual bool UpsertGachaState(const FOGGachaStateRecord&, FString&) override;
    virtual bool TryReadGachaState(const FOGEntityId&, FName, bool&, FOGGachaStateRecord&, FString&) const override;

    virtual bool UpsertLocation(const FOGLocationRecord&, int64, FString&) override;
    virtual bool TryReadLocation(const FOGEntityId&, bool&, FOGLocationRecord&, FString&) const override;
    virtual bool UpsertWorldPresence(const FOGWorldPresenceRecord&, FString&) override;
    virtual bool TryReadWorldPresence(const FOGEntityId&, bool&, FOGWorldPresenceRecord&, FString&) const override;
    virtual bool UpsertKnowledgeFact(const FOGKnowledgeFactRecord&, FString&) override;
    virtual bool TryReadKnowledgeFact(const FOGEntityId&, FName, const FOGEntityId&, bool&, FOGKnowledgeFactRecord&, FString&) const override;

    virtual bool UpsertTerritory(const FOGTerritoryRecord&, int64, FString&) override;
    virtual bool TryReadTerritory(const FOGEntityId&, bool&, FOGTerritoryRecord&, FString&) const override;
    virtual bool UpsertDomainCore(const FOGDomainCoreRecord&, int64, FString&) override;
    virtual bool TryReadDomainCore(const FOGEntityId&, bool&, FOGDomainCoreRecord&, FString&) const override;
    virtual bool SetResourceBalance(const FOGEntityId&, const FOGContentId&, int64, FString&) override;
    virtual bool TryReadResourceBalance(const FOGEntityId&, const FOGContentId&, bool&, int64&, FString&) const override;
    virtual bool UpsertProject(const FOGProjectRecord&, int64, FString&) override;
    virtual bool TryReadProject(const FOGEntityId&, bool&, FOGProjectRecord&, FString&) const override;

    virtual bool UpsertDispatch(const FOGDispatchRecord&, int64, FString&) override;
    virtual bool TryReadDispatch(const FOGEntityId&, bool&, FOGDispatchRecord&, FString&) const override;

    virtual bool UpsertFaction(const FOGFactionRecord&, int64, FString&) override;
    virtual bool TryReadFaction(const FOGEntityId&, bool&, FOGFactionRecord&, FString&) const override;
    virtual bool UpsertFactionLink(const FOGFactionLinkRecord&, FString&) override;
    virtual bool TryReadFactionLink(const FOGEntityId&, const FOGEntityId&, EOGFactionLinkType, bool&, FOGFactionLinkRecord&, FString&) const override;

    virtual bool UpsertArmy(const FOGArmyRecord&, int64, FString&) override;
    virtual bool TryReadArmy(const FOGEntityId&, bool&, FOGArmyRecord&, FString&) const override;

    virtual bool UpsertWar(const FOGWarRecord&, int64, FString&) override;
    virtual bool TryReadWar(const FOGEntityId&, bool&, FOGWarRecord&, FString&) const override;

    virtual bool UpsertContentPackage(const FOGContentId&, int32, const FString&, bool, bool, const FString&, FString&) override;
    virtual bool SetContentPackageActivated(const FOGContentId&, bool, FString&) override;
    virtual bool IsContentPackageActivated(const FOGContentId&, bool&, bool&, FString&) const override;

    virtual bool AppendWorldEvent(const FOGWorldEvent&, FString&) override;

    virtual bool BackupTo(const FString&, FString&) override;
    virtual bool RestoreFrom(const FString&, FString&) override;
    virtual bool RunIntegrityCheck(FString&, FString&) const override;
    virtual bool Checkpoint(FString&) override;

    virtual const FString& GetDatabasePath() const override { return DatabasePath; }

private:
    bool EnsureMigrationTable(FString&);
    bool ApplyMigrations(FString&);
    bool RecordMigration(int32, const TCHAR*, FString&);
    FString LastError(const TCHAR*) const;

    sqlite3* Database = nullptr;
    FString DatabasePath;
    bool bTransactionActive = false;
};
