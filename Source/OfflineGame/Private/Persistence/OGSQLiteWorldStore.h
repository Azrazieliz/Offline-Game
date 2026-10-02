#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"

struct sqlite3;

class FOGSQLiteWorldStore final : public IOGWorldStore
{
public:
    FOGSQLiteWorldStore() = default;
    virtual ~FOGSQLiteWorldStore() override;

    virtual bool Open(const FString& AbsoluteDatabasePath, FString& OutError) override;
    virtual void Close() override;
    virtual bool IsOpen() const override { return Database != nullptr; }

    virtual bool BeginTransaction(FString& OutError) override;
    virtual bool CommitTransaction(FString& OutError) override;
    virtual bool RollbackTransaction(FString& OutError) override;
    virtual int32 GetSchemaVersion(FString& OutError) const override;

    virtual bool UpsertEntity(const FOGEntityId&, FName, int64, const FString&, FString&) override;
    virtual bool TryReadEntity(const FOGEntityId&, bool&, FName&, FString&, int64&, FString&) const override;

    virtual bool UpsertCharacterManifestation(const FOGCharacterManifestationRecord&, int64, FString&) override;
    virtual bool TryReadCharacterManifestation(const FOGEntityId&, bool&, FOGCharacterManifestationRecord&, FString&) const override;

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
    bool ExecuteSql(const FString& Sql, FString& OutError) const;
    bool EnsureMigrationTable(FString& OutError);
    bool ApplyMigrations(FString& OutError);
    bool RecordMigration(int32 Version, const TCHAR* Name, FString& OutError);
    FString LastError(const TCHAR* Context) const;

    sqlite3* Database = nullptr;
    FString DatabasePath;
    bool bTransactionActive = false;
};
