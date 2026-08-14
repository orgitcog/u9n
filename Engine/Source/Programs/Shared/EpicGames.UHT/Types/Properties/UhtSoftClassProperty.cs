// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics.CodeAnalysis;
using System.Text;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// FSoftClassProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "SoftClassProperty", IsProperty = true)]
	public class UhtSoftClassProperty : UhtSoftObjectProperty
	{
		/// <inheritdoc/>
		public override string EngineClassName => "SoftClassProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "SoftClassPtr";

		/// <summary>
		/// Construct a new class property
		/// </summary>
		/// <param name="propertySettings">Property setting</param>
		/// <param name="referencedClass">Referenced class</param>
		public UhtSoftClassProperty(UhtPropertySettings propertySettings, UhtClass referencedClass)
			: base(propertySettings, UhtObjectCppForm.TSoftClassPtr, referencedClass)
		{
		}

		/// <inheritdoc/>
		public override string? GetForwardDeclarations()
		{
			return MetaClass != null ? $"class {MetaClass.SourceName};" : null;
		}

		/// <inheritdoc/>
		public override void CollectReferencesInternal(IUhtReferenceCollector collector, bool templateProperty)
		{
			collector.AddCrossModuleReference(MetaClass, false);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				default:
					builder.Append("TSoftClassPtr<").Append(MetaClass?.SourceName).Append("> ");
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FSoftClassPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FSoftClassPropertyParams", "UECodeGen_Private::EPropertyGenFlags::SoftClass");
			AppendMemberDefRef(builder, context, MetaClass, false);
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		#region Keyword
		[UhtPropertyType(Keyword = "TSoftClassPtr")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? SoftClassPtrProperty(UhtPropertyResolveArgs args)
		{
			UhtClass? metaClass = args.ParseTemplateClass();
			if (metaClass == null)
			{
				return null;
			}

			// With TSubclassOf, MetaClass is used as a class limiter.  
			return new UhtSoftClassProperty(args.PropertySettings, metaClass);
		}
		#endregion
	}
}
