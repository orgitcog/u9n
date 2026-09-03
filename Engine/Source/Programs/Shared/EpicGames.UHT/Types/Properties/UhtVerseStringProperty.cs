// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Text;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Types
{

	/// <summary>
	/// FVerseStringProperty
	/// </summary>
	[UnrealHeaderTool]
	[UhtEngineClass(Name = "VerseStringProperty", IsProperty = true)]
	public class UhtVerseStringProperty : UhtProperty
	{

		/// <summary>
		/// Defines the type of embedded character type.  Currently only u8 is used.
		/// </summary>
		public UhtProperty CharTypeProperty { get; init; }

		/// <inheritdoc/>
		public override string EngineClassName => "VerseStringProperty";

		/// <inheritdoc/>
		protected override string CppTypeText => "FVerseString";

		/// <inheritdoc/>
		protected override string PGetMacroText => "PROPERTY";

		/// <inheritdoc/>
		protected override UhtPGetArgumentType PGetTypeArgument => UhtPGetArgumentType.EngineClass;

		/// <summary>
		/// Construct a new property
		/// </summary>
		/// <param name="propertySettings">Property settings</param>
		public UhtVerseStringProperty(UhtPropertySettings propertySettings) : base(propertySettings)
		{
			UhtPropertySettings charTypePropertySettings = new();
			charTypePropertySettings.Reset(this, 0, PropertyCategory, 0);
			charTypePropertySettings.SourceName = propertySettings.SourceName;
			CharTypeProperty = new UhtByteProperty(propertySettings);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendText(StringBuilder builder, UhtPropertyTextType textType, bool isTemplateArgument)
		{
			switch (textType)
			{
				case UhtPropertyTextType.VerseMangledType:
					builder.Append("[]char");
					break;

				default:
					base.AppendText(builder, textType, isTemplateArgument);
					break;
			}
			return builder;
		}

		/// <inheritdoc/>
		protected override bool NeedsGCBarrierWhenPassedToFunctionImpl(UhtFunction function)
		{
			return CharTypeProperty.NeedsGCBarrierWhenPassedToFunction(function);
		}

		/// <inheritdoc/>
		public override void Validate(UhtStruct outerStruct, UhtProperty outermostProperty, UhtValidationOptions options)
		{
			base.Validate(outerStruct, outermostProperty, options);
			CharTypeProperty.Validate(outerStruct, outermostProperty, options);
		}

		/// <inheritdoc/>
		public override bool ScanForInstancedReferenced(bool deepScan)
		{
			return CharTypeProperty.ScanForInstancedReferenced(deepScan);
		}

		///<inheritdoc/>
		public override bool ContainsEditorOnlyProperties()
		{
			return CharTypeProperty.ContainsEditorOnlyProperties();
		}

		/// <inheritdoc/>
		public override IEnumerable<UhtType> EnumerateReferencedTypes()
		{
			foreach (UhtType type in CharTypeProperty.EnumerateReferencedTypes())
			{
				yield return type;
			}
		}

		/// <inheritdoc/>
		public override StringBuilder AppendNullConstructorArg(StringBuilder builder, bool isInitializer)
		{
			builder.AppendPropertyText(this, UhtPropertyTextType.Construction).Append("()");
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMetaDataDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			builder.AppendMetaDataDecl(CharTypeProperty, context, name, GetNameSuffix(nameSuffix, "_Inner"), tabs);
			return base.AppendMetaDataDecl(builder, context, name, nameSuffix, tabs);
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDecl(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			builder.AppendMemberDecl(CharTypeProperty, context, name, GetNameSuffix(nameSuffix, "_Inner"), tabs);
			return AppendMemberDecl(builder, context, name, nameSuffix, tabs, "FVerseStringPropertyParams");
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberDef(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, string? offset, int tabs)
		{
			builder.AppendMemberDef(CharTypeProperty, context, name, GetNameSuffix(nameSuffix, "_Inner"), "0", tabs);
			AppendMemberDefStart(builder, context, name, nameSuffix, offset, tabs, "FVerseStringPropertyParams", "UECodeGen_Private::EPropertyGenFlags::VerseString");
			AppendMemberDefEnd(builder, context, name, nameSuffix);
			return builder;
		}

		/// <inheritdoc/>
		public override StringBuilder AppendMemberPtr(StringBuilder builder, IUhtPropertyMemberContext context, string name, string nameSuffix, int tabs)
		{
			builder.AppendMemberPtr(CharTypeProperty, context, name, GetNameSuffix(nameSuffix, "_Inner"), tabs);
			base.AppendMemberPtr(builder, context, name, nameSuffix, tabs);
			return builder;
		}

		/// <inheritdoc/>
		public override void AppendObjectHashes(StringBuilder builder, int startingLength, IUhtPropertyMemberContext context)
		{
			CharTypeProperty.AppendObjectHashes(builder, startingLength, context);
		}

		/// <inheritdoc/>
		public override bool SanitizeDefaultValue(IUhtTokenReader defaultValueReader, StringBuilder innerDefaultValue)
		{
			return false;
		}

		/// <inheritdoc/>
		public override bool IsSameType(UhtProperty other)
		{
			if (other is UhtVerseStringProperty otherString)
			{
				return CharTypeProperty.IsSameType(otherString.CharTypeProperty);
			}
			return false;
		}

		[UhtPropertyType(Keyword = "verse::string", Options = UhtPropertyTypeOptions.Simple | UhtPropertyTypeOptions.Immediate)]
		[UhtPropertyType(Keyword = "FVerseString", Options = UhtPropertyTypeOptions.Simple | UhtPropertyTypeOptions.Immediate)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtProperty? VerseStringProperty(UhtPropertyResolveArgs args)
		{
			return new UhtVerseStringProperty(args.PropertySettings);
		}
	}
}
